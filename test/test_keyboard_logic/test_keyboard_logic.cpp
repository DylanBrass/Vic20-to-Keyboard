//
// Created by dylanbrass on 2026-11-14.
//

#include <cstring>
#include <unity.h>

#include "keyboard_keys.h"
#include "keyboard_logic.h"

constexpr uint8_t HID_F2 = 0x3B;
constexpr uint8_t HID_F4 = 0x3D;
constexpr uint8_t HID_F6 = 0x3F;
constexpr uint8_t HID_F8 = 0x41;

constexpr int LSHIFT_COL = 7;
constexpr int LSHIFT_ROW = 1;
constexpr int RUN_STOP_COL = 7;
constexpr int RUN_STOP_ROW = 0;
constexpr int F1_COL = 6;
constexpr int F1_ROW = 5;

static KeyboardState state;

void setUp() { memset(&state, 0, sizeof(state)); }
void tearDown() {}

// Sets the raw reading, runs get_key_action() DEBOUNCE_THRESHOLD times and returns the last event
static KeyEvent settle(const int col, const int row, const bool down)
{
    state.raw_state[col][row] = down;
    KeyEvent event = {KeyEventType::None, 0};
    for (int i = 0; i < DEBOUNCE_THRESHOLD; i++) event = get_key_action(state, col, row);
    return event;
}

#pragma region Commodore layer

void test_left_arrow_is_grave_and_cbm_left_arrow_is_caps_lock()
{
    TEST_ASSERT_EQUAL_UINT8(K_GRAVE, settle(2, 0, true).code);
    settle(2, 0, false);

    state.stable_state[CBM_COL][CBM_ROW] = true;
    const KeyEvent event = settle(2, 0, true);
    TEST_ASSERT_EQUAL(KeyEventType::Press, event.type);
    TEST_ASSERT_EQUAL_UINT8(K_CAPS, event.code);
}

void test_resolve_without_cbm_passes_codes_through()
{
    TEST_ASSERT_EQUAL_UINT8(K_F1, resolve(state, K_F1));
    TEST_ASSERT_EQUAL_UINT8(K_DOWN, resolve(state, K_DOWN));
    TEST_ASSERT_EQUAL_UINT8(K_RIGHT, resolve(state, K_RIGHT));
    TEST_ASSERT_EQUAL_UINT8(K_LALT, resolve(state, K_LALT));
    TEST_ASSERT_EQUAL_UINT8(K_LSHIFT, resolve(state, K_LSHIFT));
}

void test_resolve_with_cbm_translates_keys()
{
    state.stable_state[CBM_COL][CBM_ROW] = true;
    TEST_ASSERT_EQUAL_UINT8(HID_F2, resolve(state, K_F1));
    TEST_ASSERT_EQUAL_UINT8(HID_F4, resolve(state, K_F3));
    TEST_ASSERT_EQUAL_UINT8(HID_F6, resolve(state, K_F5));
    TEST_ASSERT_EQUAL_UINT8(HID_F8, resolve(state, K_F7));
    TEST_ASSERT_EQUAL_UINT8(K_UP, resolve(state, K_DOWN));
    TEST_ASSERT_EQUAL_UINT8(K_LEFT, resolve(state, K_RIGHT));
    TEST_ASSERT_EQUAL_UINT8(K_DEL, resolve(state, K_BKSP));
    TEST_ASSERT_EQUAL_UINT8(K_TAB, resolve(state, K_LSHIFT));
    TEST_ASSERT_EQUAL_UINT8(K_TAB, resolve(state, K_RSHIFT));
}

void test_resolve_with_cbm_leaves_other_keys_alone()
{
    state.stable_state[CBM_COL][CBM_ROW] = true;
    TEST_ASSERT_EQUAL_UINT8(K_A, resolve(state, K_A));
    TEST_ASSERT_EQUAL_UINT8(K_ENTER, resolve(state, K_ENTER));
}

void test_shift_alone_does_not_translate_keys()
{
    state.stable_state[LSHIFT_COL][LSHIFT_ROW] = true;
    TEST_ASSERT_EQUAL_UINT8(K_F1, resolve(state, K_F1));
    TEST_ASSERT_EQUAL_UINT8(K_DOWN, resolve(state, K_DOWN));
    TEST_ASSERT_EQUAL_UINT8(K_RIGHT, resolve(state, K_RIGHT));
}

void test_cbm_sends_nothing_by_itself()
{
    TEST_ASSERT_EQUAL(KeyEventType::None, settle(CBM_COL, CBM_ROW, true).type);
    TEST_ASSERT_TRUE(state.stable_state[CBM_COL][CBM_ROW]);
    TEST_ASSERT_EQUAL(KeyEventType::None, settle(CBM_COL, CBM_ROW, false).type);
    TEST_ASSERT_FALSE(state.stable_state[CBM_COL][CBM_ROW]);
}

void test_cbm_shift_sends_tab_and_plain_shift_sends_shift()
{
    TEST_ASSERT_EQUAL_UINT8(K_LSHIFT, settle(LSHIFT_COL, LSHIFT_ROW, true).code);
    settle(LSHIFT_COL, LSHIFT_ROW, false);

    state.stable_state[CBM_COL][CBM_ROW] = true;
    TEST_ASSERT_EQUAL_UINT8(K_TAB, settle(LSHIFT_COL, LSHIFT_ROW, true).code);
}

#pragma endregion

#pragma region Debouncing

void test_press_is_only_sent_after_debounce_threshold()
{
    state.raw_state[0][1] = true;

    for (int i = 0; i < DEBOUNCE_THRESHOLD - 1; i++)
    {
        TEST_ASSERT_EQUAL(KeyEventType::None, get_key_action(state, 0, 1).type);
    }

    const KeyEvent event = get_key_action(state, 0, 1);
    TEST_ASSERT_EQUAL(KeyEventType::Press, event.type);
    TEST_ASSERT_EQUAL_UINT8(K_A, event.code);
    TEST_ASSERT_TRUE(state.stable_state[0][1]);
}

void test_short_blip_is_ignored_as_bounce()
{
    state.raw_state[0][1] = true;
    for (int i = 0; i < DEBOUNCE_THRESHOLD - 1; i++) get_key_action(state, 0, 1);
    state.raw_state[0][1] = false;
    TEST_ASSERT_EQUAL(KeyEventType::None, get_key_action(state, 0, 1).type);
    TEST_ASSERT_EQUAL_UINT8(0, state.debounce_count[0][1]);

    state.raw_state[0][1] = true;
    for (int i = 0; i < DEBOUNCE_THRESHOLD - 1; i++)
    {
        TEST_ASSERT_EQUAL(KeyEventType::None, get_key_action(state, 0, 1).type);
    }
    TEST_ASSERT_FALSE(state.stable_state[0][1]);
}

void test_release_is_sent_after_debounce_threshold()
{
    settle(0, 1, true);
    const KeyEvent event = settle(0, 1, false);
    TEST_ASSERT_EQUAL(KeyEventType::Release, event.type);
    TEST_ASSERT_EQUAL_UINT8(K_A, event.code);
    TEST_ASSERT_FALSE(state.stable_state[0][1]);
}

void test_held_key_sends_nothing_more()
{
    settle(0, 1, true);
    for (int i = 0; i < 10; i++) TEST_ASSERT_EQUAL(KeyEventType::None, get_key_action(state, 0, 1).type);
}

#pragma endregion

#pragma region Sent code

void test_press_with_cbm_sends_translated_code()
{
    state.stable_state[CBM_COL][CBM_ROW] = true;
    const KeyEvent event = settle(F1_COL, F1_ROW, true);
    TEST_ASSERT_EQUAL(KeyEventType::Press, event.type);
    TEST_ASSERT_EQUAL_UINT8(HID_F2, event.code);
}

void test_release_uses_code_that_was_sent_even_if_cbm_changed()
{
    state.stable_state[CBM_COL][CBM_ROW] = true;
    TEST_ASSERT_EQUAL_UINT8(HID_F2, settle(F1_COL, F1_ROW, true).code);

    state.stable_state[CBM_COL][CBM_ROW] = false;

    const KeyEvent event = settle(F1_COL, F1_ROW, false);
    TEST_ASSERT_EQUAL(KeyEventType::Release, event.type);
    TEST_ASSERT_EQUAL_UINT8(HID_F2, event.code);
}

#pragma endregion

#pragma region Ghost detection

void test_single_key_is_not_a_ghost()
{
    state.raw_state[5][4] = true;
    TEST_ASSERT_FALSE(is_possible_ghost(state, 5, 4));
}

void test_two_other_keys_are_not_enough_for_a_ghost()
{
    state.raw_state[5][4] = true;
    state.raw_state[5][1] = true;
    state.raw_state[2][1] = true;
    TEST_ASSERT_FALSE(is_possible_ghost(state, 5, 4));
}

void test_three_corners_make_the_fourth_a_possible_ghost()
{
    state.raw_state[2][1] = true;
    state.raw_state[5][1] = true;
    state.raw_state[2][4] = true;
    state.raw_state[5][4] = true;
    TEST_ASSERT_TRUE(is_possible_ghost(state, 5, 4));
}

void test_ghost_check_works_for_every_corner()
{
    state.raw_state[2][1] = true;
    state.raw_state[5][1] = true;
    state.raw_state[2][4] = true;
    state.raw_state[5][4] = true;
    TEST_ASSERT_TRUE(is_possible_ghost(state, 2, 1));
    TEST_ASSERT_TRUE(is_possible_ghost(state, 5, 1));
    TEST_ASSERT_TRUE(is_possible_ghost(state, 2, 4));
    TEST_ASSERT_TRUE(is_possible_ghost(state, 5, 4));
}

void test_ghost_press_is_not_sent_until_pattern_disappears()
{
    TEST_ASSERT_EQUAL(KeyEventType::Press, settle(2, 1, true).type);
    TEST_ASSERT_EQUAL(KeyEventType::Press, settle(5, 1, true).type);
    TEST_ASSERT_EQUAL(KeyEventType::Press, settle(2, 4, true).type);

    TEST_ASSERT_EQUAL(KeyEventType::None, settle(5, 4, true).type);
    TEST_ASSERT_FALSE(state.stable_state[5][4]);

    settle(2, 4, false);
    const KeyEvent event = settle(5, 4, true);
    TEST_ASSERT_EQUAL(KeyEventType::Press, event.type);
    TEST_ASSERT_EQUAL_UINT8(K_O, event.code);
}

void test_releases_are_never_blocked_by_ghost_check()
{
    settle(2, 1, true);
    settle(5, 1, true);
    settle(2, 4, true);

    const KeyEvent event = settle(2, 4, false);
    TEST_ASSERT_EQUAL(KeyEventType::Release, event.type);
    TEST_ASSERT_EQUAL_UINT8(K_I, event.code);
}

#pragma endregion

#pragma region Restore

void test_restore_sends_windows_key()
{
    const KeyEvent press = settle(8, 0, true);
    TEST_ASSERT_EQUAL(KeyEventType::Press, press.type);
    TEST_ASSERT_EQUAL_UINT8(K_LGUI, press.code);

    const KeyEvent release = settle(8, 0, false);
    TEST_ASSERT_EQUAL(KeyEventType::Release, release.type);
    TEST_ASSERT_EQUAL_UINT8(K_LGUI, release.code);
}

void test_restore_is_never_a_ghost()
{
    settle(2, 1, true);
    settle(5, 1, true);
    settle(2, 4, true);
    TEST_ASSERT_FALSE(is_possible_ghost(state, 8, 0));
}

#pragma endregion

#pragma region Keymap

void test_keymap_has_no_duplicate_codes()
{
    constexpr int total = NUMBER_OF_COLS * NUMBER_OF_ROWS;
    for (int i = 0; i < total; i++)
    {
        for (int j = i + 1; j < total; j++)
        {
            const uint8_t a = keymap[i / NUMBER_OF_ROWS][i % NUMBER_OF_ROWS];
            const uint8_t b = keymap[j / NUMBER_OF_ROWS][j % NUMBER_OF_ROWS];
            if (a == K_NONE) continue;
            TEST_ASSERT_NOT_EQUAL_MESSAGE(a, b, "two switches share the same key code");
        }
    }
}

void test_plus_and_equals_are_separate_keys()
{
    TEST_ASSERT_EQUAL_UINT8(K_KPPLUS, keymap[1][7]);
    TEST_ASSERT_EQUAL_UINT8(K_EQUAL, keymap[3][6]);
}

void test_cbm_position_has_no_code_of_its_own()
{
    TEST_ASSERT_EQUAL_UINT8(K_NONE, keymap[CBM_COL][CBM_ROW]);
}

#pragma endregion

int main()
{
    UNITY_BEGIN();

    RUN_TEST(test_resolve_without_cbm_passes_codes_through);
    RUN_TEST(test_resolve_with_cbm_translates_keys);
    RUN_TEST(test_resolve_with_cbm_leaves_other_keys_alone);
    RUN_TEST(test_shift_alone_does_not_translate_keys);
    RUN_TEST(test_cbm_sends_nothing_by_itself);
    RUN_TEST(test_cbm_shift_sends_tab_and_plain_shift_sends_shift);
    RUN_TEST(test_left_arrow_is_grave_and_cbm_left_arrow_is_caps_lock);

    RUN_TEST(test_press_is_only_sent_after_debounce_threshold);
    RUN_TEST(test_short_blip_is_ignored_as_bounce);
    RUN_TEST(test_release_is_sent_after_debounce_threshold);
    RUN_TEST(test_held_key_sends_nothing_more);

    RUN_TEST(test_press_with_cbm_sends_translated_code);
    RUN_TEST(test_release_uses_code_that_was_sent_even_if_cbm_changed);

    RUN_TEST(test_single_key_is_not_a_ghost);
    RUN_TEST(test_two_other_keys_are_not_enough_for_a_ghost);
    RUN_TEST(test_three_corners_make_the_fourth_a_possible_ghost);
    RUN_TEST(test_ghost_check_works_for_every_corner);
    RUN_TEST(test_ghost_press_is_not_sent_until_pattern_disappears);
    RUN_TEST(test_releases_are_never_blocked_by_ghost_check);

    RUN_TEST(test_restore_sends_windows_key);
    RUN_TEST(test_restore_is_never_a_ghost);

    RUN_TEST(test_keymap_has_no_duplicate_codes);
    RUN_TEST(test_plus_and_equals_are_separate_keys);
    RUN_TEST(test_cbm_position_has_no_code_of_its_own);

    return UNITY_END();
}