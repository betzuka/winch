#include "heltec.h"
#include "common/WinchMessages.h"
#include "common/Timeout.h"
#include "common/SetupCommon.h"
#include "common/LED.h"
#include "common/Button.h"

#include "Battery.h"
#include "Drawing.h"
#include "Menu.h"

#define BTN_PIN                     23
#define LED_PIN                     25

#define BTN_DEBOUNCE_TIME           20
#define BTN_FAST_CLICK_TIME         250
#define BTN_MULTI_CLICK_COUNT       3
#define BTN_LONG_PRESS_TIME         750

#define MIN_PACKET_INTERVAL         200
#define MAX_PACKET_INTERVAL         500
#define LED_FLASH_TIME              50
#define SCREEN_REFRESH_INTERVAL     50

LoRaBinding loRaBinding(DEFAULT_BIND_KEY);
ButtonClass button(BTN_PIN, BTN_DEBOUNCE_TIME, BTN_FAST_CLICK_TIME, BTN_LONG_PRESS_TIME, BTN_MULTI_CLICK_COUNT);

LinkQualityClass linkQuality;

WinchStatusUncompressed winchStatus;

ControllerPacketManager controllerPacket(&loRaBinding);
LEDClass led(LED_PIN);

MenuClass menu(&controllerPacket, &winchStatus, &linkQuality, &led);

void drawScreen() {
    Heltec.display->clear();
    drawBattery(readVBatt());
    drawRSSI((int)(100*linkQuality.getSuccessRate()));
    menu.paint();
    Heltec.display->display();
}

void setup() {
    setupCommon();
    setupVBatt();
    pinMode(BTN_PIN, INPUT_PULLUP);
    pinMode(LED_PIN, OUTPUT);
}

bool receiveLoRaWinchStatus() {
    static WinchStatusCompressed compressed;
    if (loRaBinding.receivePacket((uint8_t*)&compressed, sizeof(compressed))) {
        decompressWinchStatus(&compressed, &winchStatus);
        return true;
    } 
    return false;
}

void loop() {
    static Timeout screenTimeout(0, SCREEN_REFRESH_INTERVAL);

    uint32_t now = millis();
    ButtonEvent buttonEvent = button.tick(now);

    switch (buttonEvent) {
        case ButtonEvent::SINGLE_CLICK: menu.singleClick(now); break;
        case ButtonEvent::MULTI_CLICK: menu.multiClick(now); break;
        case ButtonEvent::LONG_PRESS: menu.longPress(now); break;
        case ButtonEvent::LONG_PRESS_END: menu.longPressEnd(now); break;
    }

    menu.tick(now);

    if (controllerPacket.send(now, MIN_PACKET_INTERVAL, MAX_PACKET_INTERVAL)) {
        linkQuality.onTx(0);
    }

    bool winchStatusReceived = receiveLoRaWinchStatus();
    if (winchStatusReceived) {
        linkQuality.onRx(0);
        led.flashOnce(now, LED_FLASH_TIME);
    }

    led.tick(now);

    if (winchStatusReceived || screenTimeout.isTimeout(now)) {
        drawScreen();
        screenTimeout.reset(now);
    }

}