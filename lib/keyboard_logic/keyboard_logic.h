//
// Created by dylanbrass on 2026-11-14.
//

#ifndef VIC20_KEYBOARD_KEYBOARD_LOGIC_H
#define VIC20_KEYBOARD_KEYBOARD_LOGIC_H

#include <cstdint>

constexpr uint8_t NUMBER_OF_ROWS = 8;
constexpr uint8_t NUMBER_OF_COLS = 9;

constexpr uint8_t DEBOUNCE_THRESHOLD = 3;

constexpr uint8_t FN_COMBO_OFFSET = 1;

constexpr uint8_t CBM_COL = 3;
constexpr uint8_t CBM_ROW = 0;

struct KeyboardState
{
    // Unfiltered result of the latest scan_matrix(): true => switch is closed.
    // Can flicker from contact bounce, so it's only trusted after debouncing (see stable_state).
    bool raw_state[NUMBER_OF_COLS][NUMBER_OF_ROWS];

    // The current stable state of the key, meaning that the key raw state was stable for DEBOUNCE_THRESHOLD cycles
    bool stable_state[NUMBER_OF_COLS][NUMBER_OF_ROWS];

    // How many scans in a row raw_state has disagreed with stable_state (reset on agreement).
    // e.g. if the raw is true and stable is false, DEBOUNCE_THRESHOLD amount of cycles will cause the
    // debounce_count[x][y] to go to 1,2 and then 0 when stable
    // is switched to true and raw and stable have the same value.
    uint8_t debounce_count[NUMBER_OF_COLS][NUMBER_OF_ROWS];

    // What code the key sent to the device
    // This allows the program the release of the key, for example commodore + f1 gives f2.
    // If the user releases commodore first, and later on f1, we need to send the release of f2
    uint8_t sent_code[NUMBER_OF_COLS][NUMBER_OF_ROWS];
};

enum class KeyEventType : uint8_t
{
    None,
    Press,
    Release,
};

struct KeyEvent
{
    KeyEventType type;
    uint8_t code;
};

extern const uint8_t keymap[NUMBER_OF_COLS][NUMBER_OF_ROWS];

bool cbm_held(const KeyboardState& state);
uint8_t resolve(const KeyboardState& state, uint8_t code);
bool is_possible_ghost(const KeyboardState& state, int col, int row);
KeyEvent get_key_action(KeyboardState& state, int col, int row);

#endif //VIC20_KEYBOARD_KEYBOARD_LOGIC_H