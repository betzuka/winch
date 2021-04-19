#ifndef _MOTORCONTROLLER_h
#define _MOTORCONTROLLER_h

#include <Arduino.h>
#include "VescUart.h"
#include "RampController.h"

class MotorController {

    private:

        enum Mode {
            CURRENT_ONLY,
            CURRENT_RPM_LIMIT,
            RELEASED,
            BRAKED
        };

        Mode mode = BRAKED;
        bool rpmLimited = false;
        bool stalled = false;
        VescUart *vesc;
        RampController ampsController;
        float brakeAmps = 0;
        float lastAmpsSet = 0;
        float rpmLimit = 0;
        float maxAmps = 50;
        float maxBrakeAmps = 50;
        float minAmps = 0;
        float minBrakeAmps = 0;
        float highLimitScale = 1.1;
        long tachOffset = 0;

        void setAmps(float amps, float forceCorrectionScale) {

            float tempAmps = amps;
            amps *= forceCorrectionScale;

            if (amps>maxAmps) {
                amps = maxAmps;
            } else if (amps<minAmps) {
                amps = minAmps;
            }
            vesc->setCurrent(amps);
            lastAmpsSet = tempAmps;
        }

        void setRPM(float rpm) {
            vesc->setRPM(rpm);
        }

        void setBrake(float amps, float forceCorrectionScale) {
            amps *= forceCorrectionScale;
            if (amps>maxBrakeAmps) {
                amps = maxBrakeAmps;
            } else if (amps<minBrakeAmps) {
                amps = minBrakeAmps;
            }
            vesc->setBrakeCurrent(amps);
            lastAmpsSet = 0;
        }



    public:
        MotorController(VescUart *vesc, float currentLimit, float brakeCurrentLimit);
        void startCurrentRamp(uint32_t now, float targetAmps, uint16_t rampTime);
        void startCurrentRampLimitRPM(uint32_t now, float targetAmps, uint16_t rampTime, float rpmLimit);
        void releaseMotor();
        void brakeMotor(float brakeAmps);
        bool readVesc() {
            return vesc->getVescValues();
        }
        bool begin() {
            bool success = readVesc();
            if (success) {
                zeroTachometer();
            }
            return success;
        }
        void update(uint32_t now, float forceCorrectionScale);
        void zeroTachometer() {
            tachOffset = vesc->data.tachometer;
        }
        long getTachometer() {
            return vesc->data.tachometer - tachOffset;
        }

};


#endif