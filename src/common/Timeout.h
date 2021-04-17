#ifndef _TIMEOUT_h
#define _TIMEOUT_h
 
#include <Arduino.h>

class Timeout {
    private:
        uint32_t timeoutAt;
        uint32_t duration;
    public:
        Timeout(uint32_t now, uint32_t duration) {
            this->timeoutAt = now + duration;
            this->duration = duration;
        }
        void reset(uint32_t now) {
            this->timeoutAt = now + duration;
        }
        bool isTimeout(uint32_t now) {
            return now>timeoutAt;
        }
};


class ExpiringBoolean {
    private:
        bool val = false;
        uint32_t expiryTime;
        uint32_t serviceInterval;
    public:
        ExpiringBoolean(uint32_t serviceInterval) {
            val = false;
            expiryTime = 0;
            serviceInterval = serviceInterval;
        }
        bool isSet(uint32_t now) {
            if (val) {
                if (now>expiryTime) {
                    val = false;
                }
            }
            return val;
        }
        void set(uint32_t now, bool val) {
            this->val = val;
            if (val) {
                expiryTime = now + serviceInterval;
            }
        }
        void clear() {
            this->val = false;
        }
};

#endif