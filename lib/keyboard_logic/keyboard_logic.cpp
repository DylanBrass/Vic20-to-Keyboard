//
// Created by dylanbrass on 2026-11-14.
//
#include "keyboard_logic.h"
#include "keyboard_keys.h"

const uint8_t keymap[NUMBER_OF_COLS][NUMBER_OF_ROWS] = {
    {K_LCTRL, K_A, K_D, K_G, K_J, K_RIGHT, K_QUOTE, K_L},
    {K_1, K_3, K_5, K_7, K_9, K_BKSP, K_RALT, K_KPPLUS},
    {K_GRAVE, K_W, K_R, K_Y, K_I, K_ENTER, K_RBRACKET, K_P},
    {K_NONE, K_S, K_F, K_H, K_K, K_F3, K_EQUAL, K_SEMI},
    {K_2, K_4, K_6, K_8, K_0, K_F7, K_HOME, K_MINUS},
    {K_Q, K_E, K_T, K_U, K_O, K_F5, K_BACKSLASH, K_LBRACKET},
    {K_SPACE, K_Z, K_C, K_B, K_M, K_F1, K_RSHIFT, K_PERIOD},
    {K_LALT, K_LSHIFT, K_X, K_V, K_N, K_DOWN, K_SLASH, K_COMMA},
    {K_LGUI, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE},
};

/**
 * Returns the state of the commodore key
 * @return true if commodore key is pressed, false otherwise
 */
bool cbm_held(const KeyboardState& state)
{
    return state.stable_state[CBM_COL][CBM_ROW];
}

/**
 * Checks if the code needs to be changed because of any mutation (e.g. the commodore key)
 * @param code the code from the keymap to be verified
 * @return the code value (will be the same if it is unaffected by any mutation)
 */
uint8_t resolve(const KeyboardState& state, const uint8_t code)
{
    if (!cbm_held(state)) return code;
    switch (code)
    {
    case K_F1:
    case K_F3:
    case K_F5:
    case K_F7: return code + FN_COMBO_OFFSET;
    case K_DOWN: return K_UP;
    case K_RIGHT: return K_LEFT;
    case K_BKSP: return K_DEL;
    case K_GRAVE: return K_CAPS;
    case K_LALT: return K_ESC;
    case K_RSHIFT: return K_TAB;
    default: return code;
    }
}

/**
 * The VIC-20 keyboard matrix has no diodes, so when three keys are held, the LOW from the
 * scanned column can travel through them and make a fourth key look pressed:
 *
 *   (col K, row X) --same row--  (col J, row X)
 *         .                            |
 *         .                       same column
 *         .                            |
 *   (col K, row Y) <-- ghost     (col J, row Y)
 *
 * where K != J, X != Y, and (K,X), (J,X) and (J,Y) are pressed.
 *
 * A major trade-off is that if the 4th key (last one pressed, K,Y for example) is actually pressed
 * it will not register. I consider this to be OK for my use case.
 *
 * @param col the column of the key just detected as pressed
 * @param row the row of that key
 * @return true if the other three corners are pressed (the key may be a ghost)
 */
bool is_possible_ghost(const KeyboardState& state, const int col, const int row)
{
    for (int scanned_col = 0; scanned_col < NUMBER_OF_COLS; scanned_col++)
    {
        // Skip the tested key's column and if the raw_state of the row at the current column (scanned_col)
        // was read as false during the last scan.
        if (scanned_col == col || !state.raw_state[scanned_col][row]) continue;

        for (int scanned_row = 0; scanned_row < NUMBER_OF_ROWS; scanned_row++)
        {
            // Skip if the tested key's row
            if (scanned_row == row) continue;
            // The other two corners are both pressed: that is a rectangle.
            if (state.raw_state[col][scanned_row] && state.raw_state[scanned_col][scanned_row]) return true;
        }
    }
    return false;
}

/**
 * Decides what to send for one key, based on the latest raw_state.
 * @return the event to send (None if there is nothing to send)
 */
KeyEvent get_key_action(KeyboardState& state, const int col, const int row)
{
    const bool raw = state.raw_state[col][row];
    KeyEvent event = {KeyEventType::None, 0};

    // if the raw state is the same then nothing has changed
    // if raw and stable are true, the key is still pressed
    // if raw and stable are false the key is still not pressed
    if (raw == state.stable_state[col][row])
    {
        // Keep the debounce to 0, or set to 0 if a previous key triggered a
        // reading (raw was true), but is now saying false, this means that the reading
        // was a false positive.
        state.debounce_count[col][row] = 0;
    }
    // Increment the debounce count and if its bigger or equal to the threshold,
    // check what action to do.
    else if (++state.debounce_count[col][row] >= DEBOUNCE_THRESHOLD)
    {
        state.debounce_count[col][row] = 0;

        // If the reading is true, and we detect a possible ghost reading,
        // we ignore this key press until the ghost pattern (see is_possible_ghost)
        // disappears and a full debounce cycle passed.
        if (raw && is_possible_ghost(state, col, row)) return event;

        state.stable_state[col][row] = raw;

        if (raw)
        {
            // Get the code value, this will mutate the value if needed
            const uint8_t code = resolve(state, keymap[col][row]);
            // Save the code for later release of key
            state.sent_code[col][row] = code;
            if (code != K_NONE) event = {KeyEventType::Press, code};
        }
        else if (state.sent_code[col][row] != K_NONE)
        {
            // If the raw state is now false, return the release of the key with
            // the same code that was sent.
            event = {KeyEventType::Release, state.sent_code[col][row]};
        }
    }
    return event;
}