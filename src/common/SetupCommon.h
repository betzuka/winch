#ifndef _SETUPCOMMON_H
#define _SETUPCOMMON_H

#include "heltec.h"

#define CODING_RATE 7
#define SPREADING_FACTOR 8
#define SIGNAL_BANDWIDTH 125E3
#define TX_POWER 14
#define SYNC_WORD 0xF3
#define BAND 868E6  

#define MAGIC 0xA5

#define DEFAULT_BIND_KEY 0xE4

static void setupCommon() {
    Heltec.begin(true /*DisplayEnable Enable*/, true /*Heltec.Heltec.Heltec.LoRa Disable*/, true /*Serial Enable*/, true /*PABOOST Enable*/, BAND /*long BAND*/);

    LoRa.setSyncWord(SYNC_WORD);
    LoRa.setCodingRate4(CODING_RATE);
    LoRa.setSpreadingFactor(SPREADING_FACTOR);
    LoRa.setSignalBandwidth(SIGNAL_BANDWIDTH);
    LoRa.setTxPower(TX_POWER,RF_PACONFIG_PASELECT_PABOOST);
    LoRa.enableCrc();
    Heltec.display->init();

}


class LoRaBinding {
    private:
        int channel;
        uint8_t bindKey;
        int lastPacketRSSI;
    public:
        LoRaBinding(uint8_t bindKey = DEFAULT_BIND_KEY) {
            this->bindKey = bindKey;
        }
        void sendPacket(uint8_t* data, size_t length) {
            uint8_t cs = MAGIC;
            for (int i=0;i<length;i++) {
                cs += data[i];
            }
            cs ^= bindKey;
            LoRa.beginPacket(length+1);
            LoRa.write(cs);
            LoRa.write(data, length);
            LoRa.endPacket();
        }
        bool receivePacket(uint8_t* data, size_t length) {
            static uint8_t buf[256];
            int packetSize = LoRa.parsePacket(length+1);
            if (packetSize == length+1) {

                uint8_t csReceived = LoRa.read() & 0xFF;
                uint8_t cs = MAGIC;
                LoRa.readBytes(buf, length);
                for (int i=0;i<length;i++) {
                    cs += buf[i];
                }
                cs ^= bindKey;
                if (cs==csReceived) {
                    memcpy(data, buf, length);
                    lastPacketRSSI = LoRa.packetRssi();
                    return true;
                }
            }
            return false;
        }
        int getPacketRSSI() {
            return lastPacketRSSI;
        }
};


#endif