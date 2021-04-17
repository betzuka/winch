#ifndef _RAMPCONTROLLER_h
#define _RAMPCONTROLLER_h

#include <Arduino.h>

class RampController {

    private:
        float rampFrom = 0;
        float targetVal = 0;
        float currentVal = 0;
        float df = 0;
        uint16_t rampTime = 0;
        uint32_t rampStartedAt = 0;
        bool finished = true;
    public:
        RampController(void);
        void startRamp(uint32_t now, float fromVal, float targetVal, uint16_t rampTime);
        float calcNow(uint32_t now);
        bool isFinished();
        float getTarget();

};


#endif