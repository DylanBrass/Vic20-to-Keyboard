//
// Created by dylanbrass on 2026-11-13.
//

#ifndef VIC20_KEYBOARD_CONFIG_H
#define VIC20_KEYBOARD_CONFIG_H
#include "keyboard_logic.h"


// Serial and timing
constexpr unsigned long SERIAL_BAUD = 115200;
constexpr unsigned int COL_SETTLE_MICRO_S = 300; // wait after driving a column low

// Pins
constexpr uint8_t row_pins[NUMBER_OF_ROWS] = {4, 5, 6, 7, 15, 16, 17, 18};
constexpr uint8_t col_pins[NUMBER_OF_COLS] = {8, 9, 10, 11, 12, 13, 14, 21, 38};
#endif //VIC20_KEYBOARD_CONFIG_H