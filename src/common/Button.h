#ifndef _BUTTON_H
#define _BUTTON_H

#include <Arduino.h>

enum class ButtonEvent {
    NONE,
    SINGLE_CLICK,
    MULTI_CLICK,
    LONG_PRESS,
    LONG_PRESS_END
};

class ButtonClass {
    private:
        int pin;
        uint32_t debounceTime;
        uint32_t maxTimeBetweenClicks;
        int numClicksInMultiClick;
        uint32_t longPressTime;

        struct {
            uint8_t currentState = 0;
            uint8_t lastInputState = 0;
            uint32_t lastDebounceTime = 0;
            uint8_t pressCount = 0;
            bool longPress = false;
        } buttonState;
        enum class ButtonAction {
            NONE,
            PRESSED,
            RELEASED
        };
    public:
        ButtonClass(int pin, uint32_t debounceTime = 20, uint32_t maxTimeBetweenClicks = 250, uint32_t longPressTime = 750, int numClicksInMultiClick = 3) {
            this->pin = pin;
            this->debounceTime =debounceTime;
            this->maxTimeBetweenClicks = maxTimeBetweenClicks;
            this->longPressTime = longPressTime;
            this->numClicksInMultiClick = numClicksInMultiClick;
        }
        ButtonEvent tick(uint32_t now) {
            ButtonEvent event = ButtonEvent::NONE;
            ButtonAction action = ButtonAction::NONE;
            uint8_t inputState = digitalRead(pin);
            if (inputState != buttonState.lastInputState) {
                buttonState.lastDebounceTime = now;
            } else if (now - buttonState.lastDebounceTime > debounceTime) {
                if (buttonState.currentState != inputState) {
                    buttonState.currentState = inputState;
                    if (inputState == LOW) {
                        action = ButtonAction::PRESSED;
                    } else {
                        action = ButtonAction::RELEASED;
                    }
                }
            }
            buttonState.lastInputState = inputState;

            if (action==ButtonAction::RELEASED) {
                if (buttonState.longPress) {
                    buttonState.longPress = false;
                    event = ButtonEvent::LONG_PRESS_END;
                }
            }
            //short click is measured on release when time pressed < short click time and button has not been pressed for another 
            else if (action==ButtonAction::PRESSED) {
                buttonState.pressCount++;
            }

            if (buttonState.currentState == HIGH && now-buttonState.lastDebounceTime > maxTimeBetweenClicks) {
                //button is not pressed and it is some time since it was released so we emit an event based on the number of clicks
                if (buttonState.pressCount==1) {
                    event = ButtonEvent::SINGLE_CLICK;
                } else if (buttonState.pressCount==numClicksInMultiClick) {
                    event = ButtonEvent::MULTI_CLICK;
                }
                buttonState.pressCount = 0;
            } else if (buttonState.currentState == LOW && now-buttonState.lastDebounceTime > longPressTime) {
                //button held emit long press
                if (!buttonState.longPress) {
                    event = ButtonEvent::LONG_PRESS;
                    buttonState.longPress = true;
                }
                buttonState.pressCount = 0;
            }

            return event;
        }
};


#endif