#ifndef _LED_H
#define _LED_H

#include <Arduino.h>

class LEDClass {
    private:
        int pin;
        bool oneShot = false;
        bool flashing = false;
        bool state = false;
        uint32_t interval = 0;
        uint32_t lastSwitchTime = 0;
    public:
        LEDClass(int pin) {
            this->pin = pin;
        }
        void startBlink(uint32_t now, uint32_t interval) {
            this->interval = interval;
            this->lastSwitchTime = now;
            this->flashing = true;
            this->oneShot = false;
        }
        void off() {
            this->flashing = false;
            this->state = false;
        }
        void on() {
            this->flashing = false;
            this->state = true;
        }
        void flashOnce(uint32_t now, uint32_t duration) {
            state = true;
            interval = duration;
            lastSwitchTime = now;
            oneShot = true;
            flashing = true;
        }
        void tick(uint32_t now) {
            if (flashing) {
                if (now-lastSwitchTime > interval) {
                    state = !state;
                    lastSwitchTime = now;
                    if (oneShot & !state) {
                        flashing = false;
                        oneShot = false;
                    }
                }
            } 
            if (state) {
                digitalWrite(pin, HIGH);
            } else {
                digitalWrite(pin, LOW);
            }
        }

};

#endif
