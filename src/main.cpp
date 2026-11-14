#include <Arduino.h>

#include "config.h"
#include "USB.h"
#include "USBHIDKeyboard.h"

USBHIDKeyboard Keyboard;

KeyboardState state;

/**
 * Reads the whole keyboard once and stores the result in raw_state.
 * Rows idle HIGH through their pull-ups. We drive one column LOW at a time:
 * if a key is pressed, it connects that column to its row and pulls the row
 * LOW too. So a row reading LOW means the key is pressed.
 */
void scan_matrix()
{
    for (int c = 0; c < NUMBER_OF_COLS; c++)
    {
        digitalWrite(col_pins[c], LOW);
        pinMode(col_pins[c], OUTPUT);
        delayMicroseconds(COL_SETTLE_MICRO_S);
        // The row pin becomes low when it connects to a col pin that was set to low and as output
        for (int r = 0; r < NUMBER_OF_ROWS; r++) state.raw_state[c][r] = digitalRead(row_pins[r]) == LOW;
        pinMode(col_pins[c], INPUT_PULLUP);
    }
}

void setup()
{
    Serial.begin(SERIAL_BAUD);
    // Setup every pin to PULLUP when at idle. This means that we can pass an output low to
    // then read low. Doing it this way avoids making a circuit with pull up resistors
    // since the ESP32 does it for us with this mode.
    for (const unsigned char rowPin : row_pins) pinMode(rowPin, INPUT_PULLUP);
    for (const unsigned char colPin : col_pins) pinMode(colPin, INPUT_PULLUP);
    Keyboard.begin();
    USB.begin();
    Serial.println("VIC-20 keyboard ready");
}

void loop()
{
    scan_matrix();

    for (int c = 0; c < NUMBER_OF_COLS; c++)
    {
        for (int r = 0; r < NUMBER_OF_ROWS; r++)
        {

            const KeyEvent event = get_key_action(state, c, r);

            if (event.type == KeyEventType::Press)
            {
                Keyboard.pressRaw(event.code);
                Serial.printf("DOWN col%d row%d -> 0x%02X\n", c, r, event.code);
            }
            else if (event.type == KeyEventType::Release)
            {
                Keyboard.releaseRaw(event.code);
                Serial.printf("UP   col%d row%d -> 0x%02X\n", c, r, event.code);
            }
        }
    }
}