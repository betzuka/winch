#ifndef _BATTERY_H
#define _BATTERY_H

#include <esp_adc_cal.h>
#include "heltec.h"

static esp_adc_cal_characteristics_t *adc_chars;
#define VOLTAGE_DIVIDER 3.24
#define DEFAULT_VREF 1100  // Default VREF use if no e-fuse calibration
#define VBATT_SMOOTH 50
#define VBATT_MAX 4100
#define VBATT_MIN 3400

static void setupVBatt() {
    adc_chars = (esp_adc_cal_characteristics_t *)calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_6, ADC_WIDTH_BIT_12, DEFAULT_VREF, adc_chars);
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_1, ADC_ATTEN_DB_6);
}

static uint16_t readVBatt() {
    static uint8_t i = 0;
    static uint16_t window[VBATT_SMOOTH];
    static uint32_t sum = 0;
    static bool windowFull = false;

    pinMode(ADC1_CHANNEL_1, OPEN_DRAIN);  // ADC GPIO37
    uint16_t reading = adc1_get_raw(ADC1_CHANNEL_1);
    if (windowFull) {
        sum -= window[i];
    }
    window[i] = esp_adc_cal_raw_to_voltage(reading, adc_chars) * VOLTAGE_DIVIDER;

    sum += window[i];

    i++;
    if (i >= VBATT_SMOOTH) {
        windowFull = true;
        i = 0;
    }

    if (windowFull) {
        return round((float)sum / (float)VBATT_SMOOTH);
    } else {
        return round((float)sum / (float)i);
    }
}

#endif