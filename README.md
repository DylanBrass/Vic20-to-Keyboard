# VIC-20 USB Keyboard

[![Tests](https://github.com/DylanBrass/Vic20-to-Keyboard/actions/workflows/tests.yml/badge.svg)](https://github.com/DylanBrass/Vic20-to-Keyboard/actions/workflows/tests.yml)

Turn a Commodore VIC-20 keyboard into a modern USB keyboard using an ESP32-S3. The firmware scans the VIC-20's key
matrix and sends the keys to your computer as a standard USB HID keyboard, using the ESP32-S3's native USB.

## Features

- Scans the VIC-20's 8x8 keyboard matrix, plus RESTORE wired in as a ninth column
- Appears as a plain USB HID keyboard, with no drivers needed
- Software debouncing
- Ghost key detection, which ignores phantom presses from the matrix
- A **Commodore key layer** for keys that are separate on a PC (the Commodore key sends nothing by itself):

  | Keys                                  | Sends                       |
  |---------------------------------------|-----------------------------|
  | `Commodore + F1/F3/F5/F7`             | `F2/F4/F6/F8`               |
  | `Commodore + CRSR Down`               | `Up`                        |
  | `Commodore + CRSR Right`              | `Left`                      |
  | `Commodore + Right Shift`             | `Tab`                       |
  | `Commodore + RUN/STOP`                | `Esc`                       |
  | `Commodore + ←`                       | `Caps Lock`                 |

- `£` is `Alt`, and `RESTORE` is the Windows key (Command on a Mac)
- `←` is a backtick in en_US
- Left Shift is always a plain Shift, and Right Shift is a plain Shift unless Commodore is held
- Separate `+` and `=` keys, as on the original keyboard
- Serial debug output of every key press
- Key logic kept separate from the hardware code, with unit tests that run on your PC

## Hardware

- A Commodore VIC-20 keyboard (8 row lines + 8 column lines + the RESTORE switch)
- An ESP32-S3 board with native USB (the USB port connected to the chip's USB pins, not only a UART bridge)
- Jumper wires or a breakout for the keyboard connector

[🔗 View Interactive Keyboard Matrix & Ghosting Demo](https://DylanBrass.github.io/Vic20-to-Keyboard/matrix_keyboard_mcu_pins_ghosting.html)

[🔗 View Interactive Pull-up Schematic](https://DylanBrass.github.io/Vic20-to-Keyboard/pullup_pin_pressed_vs_not_pressed_schematic.html)

### Wiring

![Wiring](./docs/wiring.png)

RESTORE is not part of the original matrix, so it is wired as a ninth column: one of its two wires goes to the **Row 0**
line (GPIO 4, joined at the same point as that row's jumper) and the other goes to **GPIO 38**.

## Project structure

```
Vic20_Keyboard/
├── include/
│   └── config.h                  # Hardware settings: pins, serial speed, settling delay
├── lib/
│   └── keyboard_logic/           # Self-contained library, no Arduino or USB code
│       ├── keyboard_keys.h       # USB HID key codes (enum)
│       ├── keyboard_logic.h      # Matrix constants, KeyboardState, function declarations
│       └── keyboard_logic.cpp    # Keymap, debouncing, ghost detection, Commodore layer
├── src/
│   └── main.cpp                  # Hardware code: matrix scanning and sending USB events
├── test/
│   └── test_keyboard_logic/      # Unit tests for the library
└── platformio.ini
```

The dependencies run one way only: `main.cpp` uses `config.h`, which uses `keyboard_logic.h`. The library depends on
nothing outside its own folder, so it can be built and tested without the ESP32.

## Building and flashing

This is a [PlatformIO](https://platformio.org/) project.

1. Clone the repo

2. Check `platformio.ini` matches your board. Native USB needs these build flags:
   ```ini
   build_flags =
       -DARDUINO_USB_MODE=0
       -DARDUINO_USB_CDC_ON_BOOT=0
   ```
3. Build and upload:
   ```bash
   pio run -t upload
   ```
4. Open the serial monitor (115200 baud) to see key events:
   ```bash
   pio device monitor
   ```

If the board does not enter upload mode, hold **BOOT**, tap **RESET**, then release **BOOT**.

## Running the tests

The key logic is tested on your computer, with no ESP32 attached, using PlatformIO's Unity test framework:

```bash
pio test -e native
```

On Windows, the `native` environment needs a C/C++ compiler such as MinGW installed.

The tests cover:

- **Commodore layer:** F1/F3/F5/F7, Down, Right, RUN/STOP, `←` and Right Shift are translated while Commodore is held,
  other keys are left alone (including Left Shift and `£`), the layer only applies once Commodore is debounced, and the
  Commodore key sends nothing by itself
- **Special keys:** RUN/STOP is Alt (Esc with Commodore), `←` is a backtick (Caps Lock with Commodore), and Right Shift
  is Tab with Commodore, while Left Shift and Right Shift on their own are plain Shifts
- **Debouncing:** a press is only sent after `DEBOUNCE_THRESHOLD` scans, short blips are ignored (also while a key is
  held), releases are debounced too, and every key is debounced independently
- **`sent_code`:** releasing a key releases the code that was sent, even if Commodore was released first
- **Ghost detection:** three pressed corners make the fourth a possible ghost (at every corner and at the matrix edges),
  keys in a straight line are not ghosts, the ghost key is held back until the pattern disappears and then needs a full
  debounce cycle, and releases are never blocked
- **RESTORE:** it sends and releases the Windows key, and it can never be flagged as a ghost
- **Keymap:** no two switches share the same key code (empty positions are ignored), `+` and `=` are separate keys, the
  Commodore position has no code of its own, the only other empty positions are in the RESTORE column, and no Commodore
  translation duplicates the code of a plain key

The matrix scanning and USB code in `main.cpp` talk to hardware, so they can only be checked on a real board.

## How it works

All key state lives in one `KeyboardState` struct, with one entry per switch (`[column][row]`):

| Field            | Holds                                                                 |
|------------------|-----------------------------------------------------------------------|
| `raw_state`      | Unfiltered result of the latest scan (true = switch closed)           |
| `stable_state`   | Debounced state: what the program believes each key is doing          |
| `debounce_count` | How many scans in a row `raw_state` has disagreed with `stable_state` |
| `sent_code`      | The USB code sent when each key went down                             |

Each pass of `loop()` calls `scan_matrix()` to fill `raw_state`. It then calls `get_key_action()` for every key. That
function does the debouncing, ghost check and Commodore layer translation, and returns an event: nothing, a press or a
release. `main.cpp` only sends that event to the computer.

### Matrix scanning (`scan_matrix`)

The keyboard is an 8x8 grid of switches, with RESTORE as a single extra switch on a ninth column. Each key connects one
column line to one row line.

1. All lines start as inputs with pull-ups, so they idle **high**.
2. One column at a time is set to **output low**.
3. All eight rows are read. A pressed key connects its row to the low column, and the strong output pulls the row's weak
   pull-up down, so a row reading **low** means the key at that column and row is pressed.
4. The column returns to an input with pull-up, and the next column is scanned. Only one column is ever driven at a
   time.

A short settling delay (`COL_SETTLE_MICRO_S`) after driving each column avoids reading stale values on the long keyboard
wiring.

### Debouncing

Mechanical switches bounce, so a single press can read as a quick series of on/off values. A key only changes state
after its raw reading has differed from its stable state for `DEBOUNCE_THRESHOLD` consecutive scans (default 3). If the
reading goes back to normal before that, the counter resets and the blip is ignored.

### Ghost key detection (`is_possible_ghost`)

The VIC-20 matrix has no diodes. If three keys forming three corners of a rectangle are held, the low signal can travel
through them and make the fourth corner look pressed:

```
  (col K, row X) --same row--  (col J, row X)
        .                            |
        .                       same column
        .                            |
  (col K, row Y) <-- ghost     (col J, row Y)
```

Before accepting a new press, the firmware checks whether the other three corners of such a rectangle are already
pressed, and if so skips the key for now. Since `stable_state` is not updated, the key is checked again on later scans
and accepted once the ghost pattern is gone.

**Trade-off:** if all four corners really are pressed, the last one pressed is rejected, because it looks identical to a
ghost.

### Key mapping and the Commodore layer (`resolve`)

`keymap[col][row]` holds a USB HID usage code for each switch position. The VIC-20 has only four function keys
(F1/F3/F5/F7) and two cursor keys, and no Esc, Tab, Caps Lock, Up or Left key. The Commodore key acts as a layer key:
while it is held, `resolve()` translates certain codes to their PC equivalents (see the table in Features). Only the
Right Shift is translated (to Tab); the Left Shift is left unchanged while Commodore is held.

The Commodore position itself is `K_NONE`, as are the unused positions in the RESTORE column. A `K_NONE` position
changes its state but never produces a press or release event.

The VIC-20 has separate `+` and `=` keys, while a PC keyboard only has `=` (with `+` on Shift). The VIC-20's `+` key
therefore sends the keypad plus (`K_KPPLUS`), so it types `+` on its own.

Hold Commodore **before** pressing the other key. If the other key is already down when Commodore is pressed, nothing
changes, because the code was decided at the moment of the press.

### RESTORE

RESTORE is wired to its own line on the original machine, not to the matrix. Here it is connected as a ninth column
(column 8), so it goes through the same debouncing and press/release code as every other key and sends the Windows key
(Command on a Mac). Tapping it on its own opens the Start menu on Windows.

### Why `sent_code` exists

The code sent for a key can depend on whether Commodore is held at the moment of the press. The code actually sent is
therefore saved per key, so releasing a key always releases the same code that was pressed.

For example: hold Commodore, press F1 (sends F2), release Commodore, then release F1. Without `sent_code` the release
would be looked up again as plain F1, and F2 would stay stuck down on the computer.

### Sending to the computer

Codes are sent with `Keyboard.pressRaw()` and `Keyboard.releaseRaw()`, so the HID codes in `keyboard_keys.h` go to the
host unchanged. A key is sent as soon as it is accepted (after debouncing), not when it is released. The Shift, Ctrl and
Alt keys are sent as ordinary key presses as well, so the host also sees them as modifiers.

## Configuration

Hardware settings are in `include/config.h`:

| Constant                | Purpose                               | Default          |
|-------------------------|---------------------------------------|------------------|
| `row_pins` / `col_pins` | GPIO assignments                      | see wiring table |
| `COL_SETTLE_MICRO_S`    | Delay after driving a column low (µs) | 300              |
| `SERIAL_BAUD`           | Debug serial speed                    | 115200           |

Settings that belong to the keyboard logic are in `lib/keyboard_logic/keyboard_logic.h`:

| Constant                            | Purpose                                | Default |
|-------------------------------------|----------------------------------------|---------|
| `NUMBER_OF_ROWS` / `NUMBER_OF_COLS` | Matrix size (8 columns + RESTORE)      | 8 / 9   |
| `DEBOUNCE_THRESHOLD`                | Scans a change must persist            | 3       |
| `FN_COMBO_OFFSET`                   | Offset from F1/F3/F5/F7 to F2/F4/F6/F8 | 1       |
| `CBM_COL` / `CBM_ROW`               | Matrix position of the Commodore key   | (3, 0)  |

To remap a key, edit the `keymap` array in `lib/keyboard_logic/keyboard_logic.cpp` and use any code from
`keyboard_keys.h` (or add new HID codes to the enum). To change what the Commodore layer does, edit `resolve()`.

## Known limitations

- The VIC-20 has no dedicated Up, Left, Esc, Tab or Caps Lock key, which is why they live on the Commodore layer.
- Ghost detection can reject a genuine key when three other keys of the same rectangle are already held. This is an
  accepted limitation, I chose to ignore a genuine key rather than a key being sent that was never touched.

## Resources

Keyboard matrix circuit. (2021, September 7). Wikipedia. https://en.wikipedia.org/wiki/Keyboard_matrix_circuit

## Acknowledgements

Built on the Commodore VIC-20 keyboard. Commodore and VIC-20 are trademarks of their respective owners; this project is
not affiliated with them.
