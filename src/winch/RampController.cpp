#include "RampController.h"


RampController::RampController(void) {
    this->rampTime = 0;
    this->rampStartedAt = 0;
    this->targetVal = 0;
    this->currentVal = 0;
    this->df = 0;
    this->rampFrom = 0;
    this->finished = true;
}

void RampController::startRamp(uint32_t now, float fromVal, float targetVal, uint16_t rampTime) {
    this->rampTime = rampTime;
    this->rampStartedAt = now;
    this->targetVal = targetVal;
    this->df  = targetVal - fromVal;
    this->rampFrom = fromVal;
    this->finished = false;
}

float RampController::calcNow(uint32_t now) {
    uint32_t dt = now - rampStartedAt;
    if (dt>=rampTime) {
        currentVal = targetVal;
        finished = true;
    } else {
        currentVal = rampFrom + (dt * df)/(float)rampTime;
        //bit of safety checking
        if (df>0 && currentVal>targetVal) {
            currentVal = targetVal;
        } else if (currentVal<0) {
            currentVal = 0;
        }
    }

    return currentVal;
}

bool RampController::isFinished() {
    return finished;
}

float RampController:: getTarget() {
    return targetVal;
}