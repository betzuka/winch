#ifndef _WINCHMESSAGES_H
#define _WINCHMESSAGES_H

#include <Arduino.h> 
#include "heltec.h"
#include "common/SetupCommon.h"

#define NUM_FORCES 8
const int forces[NUM_FORCES] = {10,20,30,40,50,60,70,80};


typedef struct {
    uint8_t cmd:3;
    uint8_t force:5;    //force is sent in 5kg increments
}__attribute__((packed)) LoRaControllerCommand;

typedef struct {
    uint8_t state:3;
    uint8_t remainingCapacity:5;
    uint8_t launchForce;
    uint8_t currentForce;
    int8_t lineLength;
    uint8_t lineSpeed;
    float signalStrength;
}__attribute__((packed)) WinchStatusCompressed;

typedef struct {
    uint8_t state;
    float remainingCapacity;
    float launchForce;
    float currentForce;
    float lineLength;
    float lineSpeed;
    float signalStrength;
} WinchStatusUncompressed;



class LinkQualityClass {
    private:
        uint16_t window = 0;
        bool msgSent = false;
        int bitsSet = 0;        
    public:
        void onTx(int rssi) {
            if (((window >> 15) & 0x01) == 0x01) {
                bitsSet--;
            }
            window <<= 1;
            if (!msgSent) {
                window |= 1;
                bitsSet++;
            }
            msgSent = true;
        }
        void onRx(int rssi) {
            msgSent = false;
        }
        float getSuccessRate() {
            return bitsSet/16.0;
        }
};

typedef struct {
    uint8_t mode:2;
    uint8_t pulling:1;
    uint8_t forceIdx:3;
}__attribute__((packed)) ControllerPacket;

enum class ControllerMode : uint8_t {
    IDLE = 0,
    LAUNCH = 1,
    REWIND = 2
};

const char* ControllerModeNames[3] = {"IDLE", "LAUNCH", "REWIND"};

class ControllerPacketManager {
    private:
        ControllerPacket controllerPacket;
        bool changed = true;
        uint32_t lastSentAt = 0;
        uint32_t lastReceivedAt = 0;
        float lastPacketStrength;
        LoRaBinding* loRaBinding;
    public:
        ControllerPacketManager(LoRaBinding* loRaBinding) {
            controllerPacket.forceIdx = 0;
            this->loRaBinding = loRaBinding;
            setMode(ControllerMode::IDLE);
            changed = true;
        }
        void setMode(ControllerMode mode) {
            controllerPacket.mode = (uint8_t)mode;
            changed = true;
        }
        int getForce() {
            return forces[controllerPacket.forceIdx];
        }
        void incForceIdx() {
            controllerPacket.forceIdx++;
            if (controllerPacket.forceIdx>=NUM_FORCES) {
                controllerPacket.forceIdx = 0;
            }
            changed = true;
        }
        void setPulling(bool pulling) {
            controllerPacket.pulling = pulling ? 1 : 0;
            changed = true;
        }
        bool isPulling() {
            return controllerPacket.pulling == 1;
        }
        ControllerMode getControllerMode() {
            return static_cast<ControllerMode>(controllerPacket.mode);
        }
        bool send(uint32_t now, uint32_t minIntervalWhenChanged, uint32_t minIntervalUnchanged) {
            if ((changed && now-lastSentAt > minIntervalWhenChanged) || (now-lastSentAt>minIntervalUnchanged)) {
                //LoRa.beginPacket(sizeof(ControllerPacket));
                loRaBinding->sendPacket((uint8_t*)&controllerPacket, sizeof(ControllerPacket));
                //LoRa.endPacket();
                lastSentAt = now;
                changed = false;
                return true;
            }
            return false;
        }
        bool receive(uint32_t now) {
            bool received = loRaBinding->receivePacket((uint8_t *)&controllerPacket, sizeof(ControllerPacket));
            if (received) {
                lastPacketStrength = loRaBinding->getPacketRSSI();
                changed = true;
                lastReceivedAt = now;
            }
            /*
            int packetSize = LoRa.parsePacket(sizeof(ControllerPacket));
    
            if (packetSize==sizeof(ControllerPacket)) {
                LoRa.readBytes((uint8_t *)&controllerPacket, sizeof(ControllerPacket));
                lastPacketStrength = calculatePacketStrength();
                changed = true;
                lastReceivedAt = now;
                return true;
            }
            return false;
            */
            return received;
        }
        uint32_t getLastReceivedAt() {
            return lastReceivedAt;
        }
        float getLastPacketStrength() {
            return lastPacketStrength;
        }

};


static void compressWinchStatus(WinchStatusUncompressed *uncompressed, WinchStatusCompressed *compressed) {
    compressed->state = uncompressed->state;
    
    //remaining capacity is float 0->1, map onto 5 bits 0->31
    int temp = 31 * uncompressed->remainingCapacity;
    if (temp>31) {
        temp = 31;
    } else if (temp<0) {
        temp = 0;
    }
    compressed->remainingCapacity = temp;
    temp = uncompressed->launchForce;
    if (temp>255) {
        temp = 255;
    } else if (temp<0) {
        temp = 0;
    }
    compressed->launchForce = temp;
    temp = uncompressed->currentForce;
    if (temp>255) {
        temp = 255;
    } else if (temp<0) {
        temp = 0;
    }
    compressed->currentForce = temp;
    temp = (uncompressed->lineLength / 5.0) + 0.5;
    if (temp>255) {
        temp = 255;
    } else if (temp<0) {
        temp = 0;
    }
    compressed->lineLength = temp;
    temp = (uncompressed->lineSpeed / 0.2) + 0.5;
    if (temp>127) {
        temp = 127;
    } else if (temp<-128) {
        temp = -128;
    }
    compressed->lineSpeed = temp;

    compressed->signalStrength = uncompressed->signalStrength;
}

static void decompressWinchStatus(WinchStatusCompressed *compressed, WinchStatusUncompressed *uncompressed) {
    uncompressed->state = compressed->state;
    uncompressed->remainingCapacity = compressed->remainingCapacity/31.0;
    uncompressed->launchForce = compressed->launchForce;
    uncompressed->currentForce = compressed->currentForce;
    uncompressed->lineLength = 5.0 * compressed->lineLength;
    uncompressed->lineSpeed = 0.2 * compressed->lineSpeed;
    uncompressed->signalStrength = compressed->signalStrength;
}


#endif
