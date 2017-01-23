/*
 * EEZ PSU Firmware
 * Copyright (C) 2015-present, Envox d.o.o.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "psu.h"

#if OPTION_ENCODER

#include "encoder.h"

// if time between two ticks is 5ms or less then increment by 1V (100 * 0.01V)
#define CONF_ENCODER_INCREMENT_SPEED_PT1_X 5.0f
#define CONF_ENCODER_INCREMENT_SPEED_PT1_Y 10.0f
// if time between two ticks is 250ms or more then increment by 0.01V,
// for everything between 5ms and 250ms use linear interpolation
#define CONF_ENCODER_INCREMENT_SPEED_PT2_X 250.0f
#define CONF_ENCODER_INCREMENT_SPEED_PT2_Y 1.0f

#define CONF_ENCODER_DECREMENT_SPEED_PT1_X 5.0f
#define CONF_ENCODER_DECREMENT_SPEED_PT1_Y 50.0f
#define CONF_ENCODER_DECREMENT_SPEED_PT2_X 250.0f
#define CONF_ENCODER_DECREMENT_SPEED_PT2_Y 1.0f

namespace eez {
namespace psu {
namespace encoder {

// Half-step mode?
#define HALF_STEP

// define to enable weak pullups.
#define ENABLE_PULLUPS

#define DIR_CCW 0x10
#define DIR_CW 0x20

#ifdef HALF_STEP
// Use the half-step state table (emits a code at 00 and 11)
const uint8_t ttable[6][4] = {
  {0x3 , 0x2, 0x1,  0x0}, {0x23, 0x0, 0x1,  0x0},
  {0x13, 0x2, 0x0,  0x0}, {0x3 , 0x5, 0x4,  0x0},
  {0x3 , 0x3, 0x4, 0x10}, {0x3 , 0x5, 0x3, 0x20}
};
#else
// Use the full-step state table (emits a code at 00 only)
const uint8_t ttable[7][4] = {
  {0x0, 0x2, 0x4,  0x0}, {0x3, 0x0, 0x1, 0x10},
  {0x3, 0x2, 0x0,  0x0}, {0x3, 0x2, 0x1,  0x0},
  {0x6, 0x0, 0x4,  0x0}, {0x6, 0x5, 0x0, 0x10},
  {0x6, 0x5, 0x4,  0x0},
};
#endif

bool g_variableSpeed = true;
float g_speedMultiplier = 1.0f;

volatile uint8_t state = 0;
volatile int counter = 0;
volatile unsigned long g_lastTime = 0;

void interruptHandler() {
    uint8_t pinState = (digitalRead(ENC_B) << 1) | digitalRead(ENC_A);

    state = ttable[state & 0xf][pinState];
    uint8_t result = state/* & 0x30*/;
    if (result == DIR_CW || result == DIR_CCW) {
        unsigned long time = micros();
        unsigned long diff = time - g_lastTime;

        int amount = 1;
        
        if (g_variableSpeed) {
            amount = (int) roundf(
                util::clamp(
                    g_speedMultiplier * 
                    (
                        result == DIR_CW ?

                        util::remap(diff / 1000.0f, CONF_ENCODER_INCREMENT_SPEED_PT1_X, CONF_ENCODER_INCREMENT_SPEED_PT1_Y,
                                          CONF_ENCODER_INCREMENT_SPEED_PT2_X, CONF_ENCODER_INCREMENT_SPEED_PT2_Y) :

                        util::remap(diff / 1000.0f, CONF_ENCODER_DECREMENT_SPEED_PT1_X, CONF_ENCODER_DECREMENT_SPEED_PT1_Y,
                                          CONF_ENCODER_DECREMENT_SPEED_PT2_X, CONF_ENCODER_DECREMENT_SPEED_PT2_Y)
                    ),
                    1.0f,
                    100.0f
                )
            );
        }
        
        if (result == DIR_CW) {
            counter += amount;
        } else if (result == DIR_CCW) {
            counter -= amount;
        }

        g_lastTime = time;
    }
}

void init() {
#ifdef ENABLE_PULLUPS
    digitalWrite(ENC_A, HIGH);
    digitalWrite(ENC_B, HIGH);
#endif

    attachInterrupt(digitalPinToInterrupt(ENC_A), interruptHandler, CHANGE);
    attachInterrupt(digitalPinToInterrupt(ENC_B), interruptHandler, CHANGE);
}

int readAndResetCounter() {
    noInterrupts();
    int result = counter;
    counter = 0;
    //for (int i = 0; i < g_tableIndex; ++i) {
    //    DebugTraceF("%lu", g_pinStateChangedTimeTable[i]);
    //}
    //if (result != 0) {
    //    DebugTraceF("counter: %d", result);
    //}
    //g_tableIndex = 0;
    interrupts();
    return result;
}

void enableVariableSpeed(bool enable) {
    g_variableSpeed = enable;
}

void setSpeedMultiplier(float speedMultiplier) {
    g_speedMultiplier = speedMultiplier;
}

#ifdef EEZ_PSU_SIMULATOR
void addToCounter(int value) {
    counter += value;
}
#endif

}
}
} // namespace eez::psu::encoder

#endif