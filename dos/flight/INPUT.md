# Flight keyboard input

`input_open()` installs a protected-mode IRQ1 handler and returns 1 on success.
Poll `input_keys()` for the current held-key mask; call `input_close()` before
leaving flight mode. Calling `input_open()` again while active succeeds without
installing a second handler. `input_keys()` returns zero while closed.

| Bit | Keys |
| --- | --- |
| `INPUT_FORWARD` | W, Up, keypad Up |
| `INPUT_BACK` | S, Down, keypad Down |
| `INPUT_LEFT` | A, Left, keypad Left |
| `INPUT_RIGHT` | D, Right, keypad Right |
| `INPUT_EXIT` | Escape |
| `INPUT_RESET` | R |
| `INPUT_CAMERA` | Tab |
| `INPUT_FIRE` | Space |
| `INPUT_SHIELD_TEST` | H (trainer drain) |
| `INPUT_TARGET_NEXT` | T (arena target cycle) |
| `INPUT_TARGET_NEAREST` | N (arena nearest target) |
| `INPUT_LAND` | L (navigation approach / depart) |
| `INPUT_PLANET` | P (navigation destination cycle) |

The handler consumes scan-code set 1 make/break bytes directly. It tracks
physical aliases separately: releasing W does not cancel a held Up key. Key
repeat leaves the state unchanged. Space is a held level: firing stops when
Space is released, independently of thrust. Escape, R, Tab, H, T, N, L and P make edges stay
latched until the next `input_keys()` call, so a short tap between polls is delivered
once. Pause/E1 is ignored. The handler sends one
PIC end-of-interrupt and does not chain the previous IRQ1 handler after
consuming the controller byte. The previous protected-mode vector is restored
by `input_close()`; if DPMI refuses restoration, the live wrapper stays
allocated so an interrupt cannot jump into freed memory. The handler and
state are locked before installation. BIOS keyboard input is unavailable
while this handler owns IRQ1. Call the poll API from normal enabled-interrupt
code, not from another interrupt handler.

`python3 dos/flight/test_input.py` builds native decoder tests under
ASan/UBSan, compiles the DJGPP IRQ implementation, and injects overlapping
keys into a headless DOSBox window using XTest. It runs isolated from the
flight scene. The compact result is in `dos/reports/flight-input-tests.json`.
Physical keyboards and alternate controllers are not covered by that probe.
The mask coalesces multiple taps between polls. A caller comparing successive
masks can also merge release/repress entirely between polls into one hold; a
future menu/event interface should expose discrete press events separately.

## Shield trainer key

H (scan code 0x23, bit 256) drains 25% of player maximum shields once per
press. Quick taps latch like Reset/Camera; held-key repeat does not retrigger.
This is explicitly a trainer control, not a combat weapon. R restores shields.

Arena T (0x14, bit512) and N (0x31, bit1024) use the same latched make-edge
contract. The arena consumes rising edges to avoid cycling every held frame.
The decoder checks repeat/quick taps; `test_controls.py --arena` checks the actual
application cycle/reset through DOSBox. Other modes ignore these target keys.

Navigation L (0x26, bit2048) and P (0x19, bit4096) also latch make edges.
Holding L through landing does not launch from the dock. Descent/takeoff ignore
flight controls, and held controls must be released before use after departure.
`test_controls.py --navigation` and `--navigation --launch` exercise the app.
