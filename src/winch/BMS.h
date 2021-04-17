#ifndef _BMS_H
#define _BMS_H

#include <Arduino.h>
#include "HardwareSerial.h"

class BMS {
    private:
        HardwareSerial* serialPort = NULL;
        bool onNextByte(uint32_t now, uint8_t b) {

            static int ptr = 0;
            static int payloadLength = 0;
            static uint8_t state = 0;
            static uint8_t buf[256];
            static uint8_t command = 0;
            
            Serial.println(b, HEX);
            
            bool valuesUpdated = false;
            switch (state) {
                case 0:
                    
                case 1:
                    if (b==0x24) {
                        state++; 
                    
                    } else {
                        state = 0;
                    }
                    break;
                case 2:
                    //read command
                    if (b==0x56 || b==0x57 || b==0x58) {
                        command = b;
                        state++;
                    } else {
                        state = 0;
                    }
                    break;
                case 3:
                Serial.println(command, HEX);
                    //read length
                    payloadLength = b - 4;
                    ptr = 0;
                    state++;
                    break;
                case 4:
                    //reading packet
                    buf[ptr] = b;
                    ptr++;
                    if (ptr == payloadLength) {
                        //check cs
                        uint8_t cs = 0x24 + 0x24 + command;
                        for (int i=0;i<payloadLength-1;i++) {
                            cs += buf[i];
                        }
                        uint8_t csPassed = buf[payloadLength-1];
                        if (csPassed==cs) {
                            //parse out the payload
                            if (command==0x57) {
                                SOC = buf[payloadLength-2];
                                temperature1 = ((((uint16_t)buf[payloadLength-6])<<8) | buf[payloadLength-5])/10.0;
                                temperature2 = ((((uint16_t)buf[payloadLength-4])<<8) | buf[payloadLength-3])/10.0;
                                lastBMSTime = now;
                                valuesUpdated = true;
                            }
                        } else {
                            Serial.println("CS no match");
                        }
                        state = 0;
                    }
                    break;

            }
            Serial.println(state);
            return valuesUpdated;
        }
    public:
        uint32_t lastBMSTime = 0;
        uint8_t SOC;
        float temperature1 = 0, temperature2 = 0;

        void setPort(HardwareSerial* serialPort) {
            this->serialPort = serialPort;
        }

        bool update(uint32_t now) {
            bool success = false;
            while (serialPort->available()) {
                Serial.println(serialPort->read(), HEX);
                //success = success || onNextByte(now, serialPort->read() & 0xFF);
            }
            return success;
        }
};

#endif

