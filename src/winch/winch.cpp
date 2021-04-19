#include "heltec.h"
#include "BluetoothSerial.h"
#include "crc.h"
#include "VescUart.h"
#include "MotorController.h"
#include "common/Timeout.h"
#include "common/WinchMessages.h"
#include "common/SetupCommon.h"
#include "common/LED.h"
//#include "BMS.h"

#define VERSION_STRING       "v0.2"

//safety params
#define CTRL_TIMEOUT_PAUSE  2000    //if we don't hear from the controller for CTRL_TIMEOUT_PAUSE pause pulling 
//#define CTRL_TIMEOUT_STOP   5000
#define VESC_TIMEOUT        1000
#define VESC_UPDATE_INTERVAL 20     //interval between requests to talk to VESC
#define LINE_STOP_LENGTH    20      //drop line when this number of meters remain and return to idle state
#define MAX_MOTOR_CURRENT   200
#define MAX_BRAKE_CURRENT   50

//gui
#define LED_FLASH_TIME      50
#define SCREEN_REFRESH_INTERVAL 50

//pins
#define VESC_RX_PIN         23      //hardware uart1 will be remapped to these pins
#define VESC_TX_PIN         17
#define BMS_RX_PIN          36
#define LED_PIN             25

//winch physical and electrical parameters
#define KG_PER_AMP          0.6
#define AMPS_PER_KG         (1.0/KG_PER_AMP)
#define MAX_KG_FORCE        80
#define MOTOR_MAGNETS       28
#define LARGE_PULLEY_TEETH  157
#define SMALL_PULLEY_TEETH  35
#define DRUM_INNER_DIA      0.2
#define DRUM_OUTER_DIA      0.31
#define DRUM_TURNS_TOTAL    1000

//derived constants
#define TACHO_DEGREES       60      //electrical degrees per tach reading always 60 degrees for VESC
#define MOTOR_POLE_PAIRS    (MOTOR_MAGNETS>>1)
#define GEAR_RATIO          (LARGE_PULLEY_TEETH/(float)SMALL_PULLEY_TEETH)
#define TACHO_PER_MOTOR_REV ((360/TACHO_DEGREES) * MOTOR_POLE_PAIRS)  //tacho steps per motor revolution
#define TACHO_PER_DRUM_REV  (TACHO_PER_MOTOR_REV * GEAR_RATIO)          //tacho steps per drum revolution

#define LENGTH_PER_DRUM_REV (PI * ((DRUM_INNER_DIA+DRUM_OUTER_DIA)/2.0))    //this is the avg drum circumference in meters
#define LENGTH_PER_TACHO    (LENGTH_PER_DRUM_REV / TACHO_PER_DRUM_REV)
#define LINE_STOP_TACHO     (LINE_STOP_LENGTH / LENGTH_PER_TACHO)
#define DRUM_TURNS_PER_TACHO  (1.0 / TACHO_PER_DRUM_REV)

#define ERPM_TO_MOTOR_RPM   (1.0 / (float)MOTOR_POLE_PAIRS)
#define ERPM_TO_DRUM_RPM    (ERPM_TO_MOTOR_RPM / GEAR_RATIO) 
#define ERPM_TO_LINE_SPEED  ((ERPM_TO_DRUM_RPM * LENGTH_PER_DRUM_REV) / 60.0)       //divide by 60 since this is m/s

#define MOTOR_RPM_TO_ERPM   MOTOR_POLE_PAIRS
#define DRUM_RPM_TO_ERPM    (MOTOR_RPM_TO_ERPM * GEAR_RATIO)

//we derive a scaling factor for the force to apply based on how full the drum is, factor is 1 when drum is empty and increases as drum fills
#define MIN_FORCE_CORRECTION_FACTOR             0
#define MAX_FORCE_CORRECTION_FACTOR             ((DRUM_OUTER_DIA/DRUM_INNER_DIA)-1)
#define FORCE_CORRECTION_FACTOR_PER_TURN        (MAX_FORCE_CORRECTION_FACTOR/(float)DRUM_TURNS_TOTAL)

LoRaBinding loRaBinding(DEFAULT_BIND_KEY);
ControllerPacketManager packet(&loRaBinding);
BluetoothSerial ESP_BT; //Object for Bluetooth
VescUart vesc;
MotorController motor(&vesc, MAX_MOTOR_CURRENT, MAX_BRAKE_CURRENT);
LEDClass led(LED_PIN);
//BMS bms;

class ControllerStateManager {
    private:
        char lineBuf[25];

        struct {
            int launchPullForce = forces[0];
            int launchBaseForce = 5;
            int rewindForce = 5;
            int launchTakeupRampTime = 1000;
            int launchPullRampUpTime = 5000;
            int launchPullRampDownTime = 1000;
            int rewindRampUpTime = 1000;
            int rewindMaxRPM = 35;
            int launchTakeupMaxRPM = 20;
            int brakeForce = 10;
        } winchConfig;

        struct {
            float currentForce = 0;
            float drumRPM = 0;
            float remainingLine = 0;
            float lineSpeed = 0;
            float drumTurns = 0;
            float forceCorrectionScale = 1.0;
        } winchStats;

        struct {
            ControllerMode mode = ControllerMode::IDLE;
            bool pulling = false;
            bool failsafe = false;
            bool vescOk = false;
            uint32_t lastPacketReceivedAt = 0;
        } winchState;

        struct {
            float remainingCapacity = 1;
        } bmsStats;
        

        //+TODO
        //IMPROVED LAUNCH SEQUENCE
        //On entering LAUNCH mode we are in a pre-launch state
        //In this state when the button is pressed the winch will pull at a limited speed to 1/3rd launch force
        //The pilot is expected to hold this force.
        //To initiate the launch the pilot would begin to run, after X meters of line has been pulled in the winch will build to the full force
        //Releasing the button before triggering the full launch returns the state to IDLE.

        //Alternative implementation, launch force is limited to 1/3rd full force AND speed limited until X meters of line has been wound in measured FROM the point where the pre-tension force is first registered and the speed is zero.
        //
        void update(uint32_t now, ControllerMode newMode, bool newPulling, bool init=false) {

            if (init || winchState.mode!=newMode) {
                winchState.mode = newMode;
                if (newMode==ControllerMode::LAUNCH) {
                    //start slack take up, ramp to launch base force with limited RPM to avoid ground jerk
                  //  log("Taking up slack");
                    motor.startCurrentRampLimitRPM(now, winchConfig.launchBaseForce * AMPS_PER_KG, winchConfig.launchTakeupRampTime, winchConfig.launchTakeupMaxRPM * DRUM_RPM_TO_ERPM);
                } else {
                    //brake motor
                   // log("braked");
                    motor.brakeMotor(winchConfig.brakeForce * AMPS_PER_KG);
                }
            } else if (winchState.pulling!=newPulling) {
                winchState.pulling = newPulling;
                if (winchState.mode==ControllerMode::LAUNCH) {
                    if (winchState.pulling) {
                        //start force ramp to launch pull force
                      //  log("Launch force");
                        motor.startCurrentRamp(now, winchConfig.launchPullForce * AMPS_PER_KG, winchConfig.launchPullRampUpTime);
                    } else {
                        //start force ramp to launch base force
                       // log("Base force");
                        motor.startCurrentRamp(now, winchConfig.launchBaseForce * AMPS_PER_KG, winchConfig.launchPullRampDownTime);
                    }
                } else if (winchState.mode==ControllerMode::REWIND) {
                     if (winchState.pulling) {
                        //start force ramp to rewind force with limited RPM
                       // log("rewinding");
                        motor.startCurrentRampLimitRPM(now, winchConfig.rewindForce * AMPS_PER_KG, winchConfig.rewindRampUpTime, winchConfig.rewindMaxRPM * DRUM_RPM_TO_ERPM);
                    } else {
                        //brake motor
                        //log("braked");
                        motor.brakeMotor(winchConfig.brakeForce * AMPS_PER_KG);
                    }
                }
            }

            if (winchState.mode==ControllerMode::LAUNCH && winchStats.remainingLine<=LINE_STOP_LENGTH) {
                //stop the motor to avoid overwinding overriding any previous motor instructions
                motor.brakeMotor(winchConfig.brakeForce * AMPS_PER_KG);
            } 

            

        }

        void sendWinchStatusToController() {
            static WinchStatusUncompressed uncompressed;
            static WinchStatusCompressed compressed;
            uncompressed.currentForce = winchStats.currentForce;
            uncompressed.launchForce = winchConfig.launchPullForce;
            uncompressed.lineLength = winchStats.remainingLine;
            uncompressed.lineSpeed = winchStats.lineSpeed;
            uncompressed.remainingCapacity = bmsStats.remainingCapacity;
            uncompressed.state = (uint8_t)winchState.mode;
            uncompressed.signalStrength = packet.getLastPacketStrength();
            compressWinchStatus(&uncompressed, &compressed);
            loRaBinding.sendPacket((uint8_t*)&compressed, sizeof(compressed));
        }

       

    public:
        ControllerStateManager() {
            update(millis(), ControllerMode::IDLE, false, true);
        }

        void paint() {
            static char textBuf[25];

            Heltec.display->clear();
            Heltec.display->setFont(ArialMT_Plain_10);
            Heltec.display->setTextAlignment(TEXT_ALIGN_LEFT);
            sprintf(textBuf, "%s VESC: %s CTRL: %s", VERSION_STRING, (winchState.vescOk ? "OK" : "ERR"), (!winchState.failsafe ? "OK" : "ERR"));
            
            Heltec.display->drawString(0,0,textBuf);

            Heltec.display->setFont(ArialMT_Plain_16);
            Heltec.display->setTextAlignment(TEXT_ALIGN_CENTER);

            if (winchState.pulling) {
                Heltec.display->fillRect(0,15,128,16);
                Heltec.display->setColor(BLACK);
                Heltec.display->drawString(64, 15, ControllerModeNames[(int)winchState.mode]);
                Heltec.display->setColor(WHITE);
            } else {
                Heltec.display->drawString(64, 15, ControllerModeNames[(int)winchState.mode]);
            }
            
            Heltec.display->setTextAlignment(TEXT_ALIGN_LEFT);
            sprintf(textBuf, "%d / %dKg", (int)winchStats.currentForce, (int)winchConfig.launchPullForce);
            Heltec.display->drawString(0, 31, textBuf);
            sprintf(textBuf, "%dm", (int)winchStats.remainingLine);
            Heltec.display->drawString(0, 47, textBuf);
            

            Heltec.display->setTextAlignment(TEXT_ALIGN_RIGHT);
            sprintf(textBuf, "%dm/s", (int)winchStats.lineSpeed);
            Heltec.display->drawString(127, 31, textBuf);
            sprintf(textBuf, "%d%%", (int)(bmsStats.remainingCapacity*100));
            Heltec.display->drawString(127, 47, textBuf);

            Heltec.display->display();
        }

        bool tick(uint32_t now, bool init=false) {
            static Timeout vescPoller(0, VESC_UPDATE_INTERVAL);

            bool refreshVesc = vescPoller.isTimeout(now);
            if (refreshVesc) {
                winchState.vescOk = motor.readVesc();
                float amps = vesc.data.avgMotorCurrent;
                long tacho = -motor.getTachometer();
                long erpm = vesc.data.rpm;
                
                
                winchStats.remainingLine = tacho * LENGTH_PER_TACHO;
                winchStats.drumRPM = erpm * ERPM_TO_DRUM_RPM;
                winchStats.lineSpeed = erpm * ERPM_TO_LINE_SPEED;
                winchStats.drumTurns = tacho * DRUM_TURNS_PER_TACHO;

                //calculate remaining turns on drum
                float forceCorrectionFactor = FORCE_CORRECTION_FACTOR_PER_TURN * (DRUM_TURNS_TOTAL - winchStats.drumTurns);

                if (forceCorrectionFactor<MIN_FORCE_CORRECTION_FACTOR) {
                    forceCorrectionFactor = MIN_FORCE_CORRECTION_FACTOR;
                } else if (forceCorrectionFactor>MAX_FORCE_CORRECTION_FACTOR) {
                    forceCorrectionFactor = MAX_FORCE_CORRECTION_FACTOR;
                }

                winchStats.forceCorrectionScale = 1.0 + forceCorrectionFactor;

                winchStats.currentForce = (amps * KG_PER_AMP) / winchStats.forceCorrectionScale;

                //during rewind if remaining line goes negative we zero the tacho, this allows us to reset the line length
                if (winchState.mode==ControllerMode::REWIND && winchStats.remainingLine<0) {
                    motor.zeroTachometer();
                    winchStats.remainingLine = 0;
                }

                vescPoller.reset(now);
            }

            
            bool received = packet.receive(now);
            if (received) {
                winchState.failsafe = false;
                //always take the latest force sent to us
                winchConfig.launchPullForce = packet.getForce();
                update(now, packet.getControllerMode(), packet.isPulling());
                sendWinchStatusToController();
                led.flashOnce(now, LED_FLASH_TIME);
            } else {
                //we use double checked timeout for failsafe to avoid coming out of failsafe when clock wraps back thru the last received time
                if (!winchState.failsafe && now-packet.getLastReceivedAt() > CTRL_TIMEOUT_PAUSE) {
                    winchState.failsafe = true;
                    refreshVesc = true;
                }
                if (winchState.failsafe) {
                    //special update without changing mode but forcing pull to stop
                    update(now, winchState.mode, false);
                }
            }

            if (received || refreshVesc) {
                motor.update(now, winchStats.forceCorrectionScale);
            }

            return received;
        }

        
};


void setup() {
    Serial.begin(115200);
    setupCommon();
    pinMode(LED_PIN, OUTPUT);
    Serial1.begin(115200, SERIAL_8N1, VESC_RX_PIN, VESC_TX_PIN);
   // Serial2.begin(115200, SERIAL_8N1, BMS_RX_PIN, -1, false);
    vesc.setSerialPort(&Serial1);
   // bms.setPort(&Serial2);
    while (!motor.begin()) {
        delay(10);
    }

}

void loop() {
    static Timeout screenTimeout(0, SCREEN_REFRESH_INTERVAL);
    static ControllerStateManager controllerStateManager;
    uint32_t now = millis();
/*
    if (bms.update(now)) {

        Serial.print("SOC: ");
        Serial.print(bms.SOC);
        Serial.print(" , T1: ");
        Serial.print(bms.temperature1);
        Serial.print(", T2: ");
        Serial.println(bms.temperature2);

    }*/
    controllerStateManager.tick(now);

    led.tick(now);

    if (screenTimeout.isTimeout(now)) {
        controllerStateManager.paint();
        screenTimeout.reset(now);
    }

    

}