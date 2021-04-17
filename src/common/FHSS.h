#ifndef _FHSS_H
#define _FHSS_H

#include "heltec.h"

#define NUM_HOPS 24
#define HOP_INTERVAL 100000
#define ACK_WAIT_TIME 50000
#define SETTLE_TIME  20000
#define AIR_TIME_MASTER 1000
#define AIR_TIME_SLAVE 1000

#define CYCLE_INTERVAL (NUM_HOPS*HOP_INTERVAL)

class FHSSBaseClass {
    protected:
        
        uint8_t channel = 0;
        void hop() {
            channel++;
            if (channel>=NUM_HOPS) {
                channel = 0;
            }
        }   
};

class FHSSMasterClass : public FHSSBaseClass {

    private:
        uint32_t lastHopTime = 0;
        bool slotUsed = false;
    public:

        void tick() {
            uint32_t now = micros();
            uint32_t timeInSlot = now - lastHopTime;
            if (timeInSlot>=SETTLE_TIME) {
                if (timeInSlot<HOP_INTERVAL) {
                    if (!slotUsed) {
                        //tx();
                        slotUsed = true;
                    } else {
                        //rx()
                    }
                } else {
                    hop();
                    lastHopTime = now;
                    slotUsed = false;
                }
            }
        }

};

class FHSSSlaveClass : public FHSSBaseClass {
    private:
        bool resync = true;
        uint8_t emptySlotCount = 0;
        uint32_t receivedAt = 0;
        uint32_t hopInterval = CYCLE_INTERVAL;
        uint32_t nextHopTime = 0;
    public:
        void tick() {
            uint32_t now = micros();
            if (now>nextHopTime) {
                emptySlotCount++;
                if (emptySlotCount>=NUM_HOPS) {
                    emptySlotCount = 0;
                    hopInterval = CYCLE_INTERVAL;
                }
                hop();
            }
            //rx();
            bool received = false; // = rx();
            if (received) {
                receivedAt = now;
                //tx();
                hop();
                //when we received the message (before we sent the response) master was SETTLE_TIME + AIR_TIME_MASTER into the slot
                //therefore we can calculate when the master will hop to the next channel
                nextHopTime = (now - SETTLE_TIME - AIR_TIME_MASTER) + hopInterval;
                emptySlotCount = 0;
                hopInterval = HOP_INTERVAL;
            }
            
        }
};

#endif

