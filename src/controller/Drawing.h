#ifndef _DRAWING_H
#define _DRAWING_H

#include "heltec.h"
#include "Battery.h"

#define VBATT_DISPLAY_BARS 15
#define VBATT_DISPLAY_WIDTH (VBATT_DISPLAY_BARS + 4)
#define VBATT_DISPLAY_LEFT (127 - VBATT_DISPLAY_WIDTH - 1)
#define VBATT_DISPLAY_TEXT_LEFT (VBATT_DISPLAY_LEFT - 4 * 6)

static void drawBattery(uint16_t voltage) {
    Heltec.display->setTextAlignment(TEXT_ALIGN_RIGHT);
    Heltec.display->setFont(ArialMT_Plain_10);
    Heltec.display->drawString(VBATT_DISPLAY_LEFT - 1, 0, String(voltage / 1000.0, 1) + "v");

    if (voltage > VBATT_MAX) voltage = VBATT_MAX;
    if (voltage < VBATT_MIN) voltage = VBATT_MIN;

    int bars = map(voltage, VBATT_MIN, VBATT_MAX, 0, VBATT_DISPLAY_BARS);

    Heltec.display->drawRect(VBATT_DISPLAY_LEFT, 0, VBATT_DISPLAY_WIDTH, 10);
    if (bars > 0) {
        Heltec.display->fillRect(VBATT_DISPLAY_LEFT + 2, 2, bars, 6);
    }
    Heltec.display->fillRect(126, 3, 2, 4);
}

static void drawRSSI(uint16_t rssi) {
    int v = map(rssi, 0, 100, 0, 10);

    for (int i = 0; i < v; i++) {
        Heltec.display->fillRect(3 * i, 10 - i, 2, i);
    }

    Heltec.display->setTextAlignment(TEXT_ALIGN_LEFT);
    Heltec.display->setFont(ArialMT_Plain_10);
    Heltec.display->drawString(32, 0, String(rssi) + "%");
}

#endif