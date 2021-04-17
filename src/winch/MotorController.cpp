#include "MotorController.h"


MotorController::MotorController(VescUart *vesc, float currentLimit, float brakeCurrentLimit) {
    this->vesc =  vesc;
    this->maxAmps = currentLimit;
    this->maxBrakeAmps = brakeCurrentLimit;
}

void MotorController::releaseMotor() {
    this->mode = RELEASED;
}

void MotorController::brakeMotor(float brakeAmps) {
    this->brakeAmps = brakeAmps;
    this->mode = BRAKED;
}

void MotorController::startCurrentRamp(uint32_t now, float targetAmps, uint16_t rampTime) {
    ampsController.startRamp(now, lastAmpsSet, targetAmps, rampTime);
    mode = CURRENT_ONLY;
}

void MotorController::startCurrentRampLimitRPM(uint32_t now, float targetAmps, uint16_t rampTime, float rpmLimit) {
    this->rpmLimit = rpmLimit;
    ampsController.startRamp(now, lastAmpsSet, targetAmps, rampTime);
    mode = CURRENT_RPM_LIMIT;
    //always start with the current ramp
    rpmLimited = false;
}



void MotorController::update(uint32_t now) {


    if (mode==BRAKED) {
        setBrake(brakeAmps);
    } else if (mode==RELEASED) {
        setAmps(0);
    } else if (mode==CURRENT_ONLY) {
        float newAmps = ampsController.calcNow(now);
        setAmps(newAmps);
    } else if (mode==CURRENT_RPM_LIMIT) {
        float newAmps = ampsController.calcNow(now);
        //apply some hysterisis (highLimitScale is >1)
        float rpmLimitHigh = highLimitScale * rpmLimit;
        float currentLimitHigh = highLimitScale * ampsController.getTarget();

        if (rpmLimited && vesc->data.avgMotorCurrent>currentLimitHigh) {
            rpmLimited = false;
        } else if (!rpmLimited && vesc->data.rpm>rpmLimitHigh) {
            rpmLimited = true;
        }
        if (rpmLimited) {
            setRPM(rpmLimit);
        } else {
            setAmps(newAmps);
        }
    }

}

