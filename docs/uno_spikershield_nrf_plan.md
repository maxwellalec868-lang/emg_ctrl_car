# Plan: EMG from the Uno + Heart & Brain SpikerShield stack to the STM32 car

Goal: the Uno (with 2 classic Heart & Brain SpikerShields stacked) samples EMG on A0/A1. A MATLAB
script on the laptop runs the control algorithm and sends the 1-byte drive command to the Nano ESP32,
which transmits it to the STM32 over the nRF24L01+. The STM32 side is **unchanged**: it runs
`demo_full_PC_car_ctrl()` (`stm_car_proj/Demos/full_drive_demo.c`), which takes the same byte
`pc_drive.py` sends.

**Both boards plug into the laptop over USB. There are no wires between them.**

```
 Shield 1 ─A0─┐                         ┌──────── laptop ────────┐
              ├─ Uno ──USB (1 kHz)────► │ MATLAB run_emg_car     │ ──USB (1 byte/50 ms)──► Nano ESP32 ──nRF──► STM32
 Shield 2 ─A1─┘                         │ algorithm + live plot  │                     (nrf_tx_drive)   (speed + steering)
                                        └────────────────────────┘
```

| File | Runs on | Purpose |
|---|---|---|
| `arduino_emg_acquisition/full_2_channel_wireless_ctrl_alg/full_2_channel_wireless_ctrl_alg.ino` | Uno | Samples A0/A1 at 1 kHz and streams them over USB |
| `arduino_emg_acquisition/nrf_tx_drive/nrf_tx_drive.ino` | Nano ESP32 | **Existing, unchanged:** forwards every USB byte over the nRF |
| `matlab_ACQ_CA/run_emg_car.m` (+ the rest of `matlab_ACQ_CA/`) | MATLAB | Calibration, Kirby's controllers, mappings, live drive window. **See `matlab_ACQ_CA/README.md`** |

Status:
- The Uno sketch compiles with no warnings (`arduino:avr:uno`).
- Both MATLAB files pass Code Analyzer (R2026a) with 0 messages.
- `parse_emg_chunk` passed 10 unit tests: clean, split, status, garbage and reset fragments, plus 1000
  lines in about 3 ms.
- **Not run on the hardware yet.**

Why two USB boards and not a wire between them: no divider or shared-ground wiring, the Uno uses no
pins beyond A0/A1, and the Nano firmware is the same `nrf_tx_drive` that already drove the car with
`pc_drive.py`. Each board can also be tested on its own.

---

## Part 1: Pins, wiring and power

### 1.1 What the SpikerShields put on the Uno's pins

Checked against all four official schematics of the classic Heart & Brain SpikerShield (v1.0 Nov
2014, v1.1, v1.3 Jun 2015, v1.51 SMT; `backyardbrains.com/products/files/`) and every sketch in
`BackyardBrains/SpikerShield/Heart-And-Brain`:

| Uno pin | On every shield revision |
|---|---|
| A0–A5 | Channel-select jumper (J2/JP9): the shield's output goes to whichever pin is jumpered. Yours: bottom shield on A0, top on A1 |
| D0, D1 | Nothing on the shield (USB serial to the PC) |
| D2, D5 | Nothing |
| D3 | Relay coil (v1.0/v1.1 only) |
| D4, D7 | Buttons: 5V → button → pin, 10 kΩ pull-down |
| D6 | Speaker, pin → speaker → GND |
| D8–D13 | **One LED per pin, straight to GND, no resistor** (LED1–LED6) |
| 3.3V | Nothing |

**This setup uses only A0, A1 and USB on the Uno**, so it can't conflict with anything on the shields,
and it works with any revision. No Backyard Brains Heart & Brain firmware uses the LEDs, buttons or speaker.
BYB's Uno sketch for the shield (`SpikeRecorderSpikerShield_V1_1`) reads A0–A5 and pulses D5 as an
event marker. The "carrier on D10" firmware (`HeartAndBrainSpikerBoxUno`) is for the standalone
SpikerBox, which has its own ATmega328PB, and doesn't apply to this shield.

### 1.2 Why the radio can't go on the Uno

The nRF24L01+ talks over SPI, and the Uno's hardware SPI exists **only** on D11 (MOSI), D12 (MISO) and
D13 (SCK). The ICSP header is the same copper, not a separate set of pins. Each of those pins has a
shield LED going straight to GND with no resistor, on both of your shields:

- An LED with no resistor holds its pin at about its forward voltage, roughly 2 V.
- **SCK and MOSI:** the Uno tries to drive 5 V, but the LEDs hold the line near 2 V. The nRF needs at
  least 2.31 V (0.7 × 3.3 V) to read a HIGH, so clock and data bits get lost. The pin also has to source
  well past the ATmega's 40 mA absolute maximum.
- **MISO:** the nRF answers with a weak 3.3 V signal. The LEDs pull it to about 1.7 V. The Uno needs
  3.0 V to read a HIGH, so it reads all zeros and the radio looks "not detected".
- Bit-banged SPI on the free A2–A5 pins would avoid the LEDs. But the ATmega328P datasheet warns that
  switching digital outputs on the analog port during a conversion adds noise to A0/A1.

So the radio stays on the Nano ESP32, where it already works.

### 1.3 Wiring and power

**Between the Uno and the Nano: nothing.** Each board gets its own USB cable to the laptop (a powered
USB hub works too).

**Nano ESP32 → nRF (unchanged from your working setup):**

| HW-200 adapter / nRF | Nano ESP32 |
|---|---|
| VCC | **VBUS (5 V)**. The HW-200's AMS1117 regulator needs 5 V in and gives the PA+LNA its 3.3 V. (A bare module without the adapter goes on 3V3.) |
| GND | GND |
| CE | D9 |
| CSN | D10 |
| SCK | D13 |
| MOSI | D11 |
| MISO | D12 |
| IRQ | not connected |

**Power:**
- The Uno and shields draw about 60 mA. The Nano and PA+LNA radio draw up to about 200 mA in TX. Both
  are well within a USB-A port's 500 mA.
- Keep `RF24_PA_LOW` (already set in `nrf_tx_drive`).

**Two boards on one laptop:** each shows up as its own COM port, and only one program can hold each
port. Close the Arduino Serial Monitors before starting MATLAB.
- To tell the ports apart, run `run_emg_car` with no arguments: it lists them.
- Or unplug one board and see which port disappears. In Device Manager, the Uno shows as
  "Arduino Uno" (or "USB-SERIAL CH340" on a clone), and the Nano ESP32 shows as a USB serial device.

**Safety:** Backyard Brains recommends running the laptop **on battery (unplugged from the wall)**
while electrodes are on a person. The SpikerShields are not isolated from the USB ground.

### 1.4 Bring-up and test

1. **Nano + radio (already proven):** flash `nrf_tx_drive.ino` to the Nano. STM32 runs
   `demo_full_PC_car_ctrl()`, wheels off the ground. Run `python pc_drive.py COMy`: W/A/S/D should move
   the car and Space should stop it. Close the script.
2. **Uno:** flash `full_2_channel_wireless_ctrl_alg.ino` (Board = Arduino Uno). Open the Serial Monitor
   at **250000**. You should see `# READY ...`, a stream of `0512 0498`-style lines, and
   `# overflow=0` once a second. Touch the electrodes or flex, and the numbers on that channel should
   change. **Close the Serial Monitor.**
3. **USB flow test (no radio, no car):** this proves the Uno → MATLAB → Nano path on its own.
   - Flash `usb_flow_test/uno_usb_counter` to the Uno and `usb_flow_test/nano_usb_echo` to the Nano.
     The Nano's LED turns blue.
   - In MATLAB, `cd` to `arduino_emg_acquisition/usb_flow_test` and run
     `usb_flow_test("COMx", "COMy")`. Expect `PASS` (about 1000 lines/s, 0 lost, 0 corrupted, every
     Nano reply correct).
   - Afterwards, flash `full_2_channel_wireless_ctrl_alg` back onto the Uno and `nrf_tx_drive` back
     onto the Nano.
4. **Calibrate and dry run, from MATLAB:** `cd` to `matlab_ACQ_CA`, then:
   - `run_emg_car` lists the ports.
   - `run_emg_car("COMx", "COMy")`: view signals, then a new calibration (try 3 contractions). Check
     the review plots and save. Press Enter for the default controller and mapping, and answer `n`
     to "Drive LIVE?" (dry run).
   - Expected: the EMG and controller plots respond to flexing, the title shows about 1000
     samples/s, "mapping says" changes (A0 → FWD, A1 → SLOW), the Nano's LED is green, and STM32
     `cmd_rx_count` climbs at about 20/s while the car stays stopped.
   - Close the drive window, then check the "longest gap between commands" line (should be well
     under 150 ms).
5. **Noise check:** compare the A0/A1 traces with the Nano plugged in and unplugged.
6. **Live:** choose 1 (drive again) and answer `y`, with the wheels off the ground first. The window
   starts in E-STOP: click RESUME to drive. Try the E-STOP button and spacebar.

---

## Part 2: Software

### 2.1 Uno sketch: `full_2_channel_wireless_ctrl_alg.ino`

Sketch name note: you asked for `full_2_channel_wireless_ctrl_alg.ioc`. `.ioc` is a CubeMX file, so the
Arduino sketch is `.ino`, and it lives in a folder with the same name (the Arduino IDE requires that).
The wireless part of the name now refers to the whole system: this sketch only acquires.

| 3-channel sketch | 2-channel sketch |
|---|---|
| `N_CHANS 3`, pins `{A0, A1, A2}` | `N_CHANS 2`, pins `{A0, A1}` |
| `"%04d %04d %04d"`, buffer 20 | `"%04d %04d"`, buffer 12 |
| `MATLAB_BAUD_RATE 250000` | `PC_BAUD_RATE 250000` (same speed; exact on a 16 MHz Uno) |
| `micros()` busy-wait timing in `loop()` | **Timer1 interrupt** at 1 kHz + 64-sample ring buffer |
| no status | `# READY` at boot, `# overflow=N` once a second |

**Why the timer interrupt:** `Serial.println()` has to wait whenever the 64-byte USB transmit buffer
fills up. In the old busy-wait loop, that wait would shift the sample timing. With the interrupt,
samples stay exactly 1 ms apart, and the ring buffer absorbs up to 64 ms of slow reading.

### 2.2 Serial contract

**Uno port** (250000 baud, 8N1, the Uno → PC direction only):
- One line per sample: `"AAAA BBBB\r\n"`, where AAAA = A0 and BBBB = A1, each **exactly 4 digits**,
  0000–1023. 1000 lines/s, about 44 % of the link.
- Lines starting with `#` are status messages, not samples.
- Opening the port **resets the Uno** (DTR). Wait about 2 s, then flush. `run_emg_car.m` does this once
  and keeps the port open for the whole session.

**Nano port** (115200, the PC → Nano direction only):
- **One raw byte** per decision, the same bitmask as `pc_drive.py`:
  `0x01` forward, `0x02` slow, `0x04` left, `0x08` right, `0x10` E-STOP.
- Send one roughly every **50 ms** (the STM32 control tick). Never let the gap reach **200 ms**, or the
  STM32 failsafe E-stops the car.
- `nrf_tx_drive` forwards each byte immediately as a 1-byte radio packet.

### 2.3 MATLAB: `matlab_ACQ_CA/`

The full guide is in **`matlab_ACQ_CA/README.md`**. In short:

- **`run_emg_car("COM_uno", "COM_nano")`** is the one entry point. It replaces running Kirby's
  sections by hand. It opens both ports once, then:
  1. Optional live view.
  2. Calibration: new, load or revise. This is Kirby's Sections 1A/1B with the same protocol, math,
     review and `.mat` files.
  3. Pick the controller and the mapping.
  4. Dry run or LIVE drive window.
  5. Repeat or quit.
- **Processing ("clean"):** Kirby's code has no digital filter, since the SpikerShield hardware
  band-passes the signal (about 1–130 Hz). The MATLAB steps are:
  1. Counts → volts (`counts/1023*5 - 2.5`).
  2. 250 ms moving RMS.
  3. Normalise between the user's rest and contraction RMS from calibration.
  4. Smoothing (control03b).
- **Controllers:** Kirby's `control03b` (default), `control01` and `control04`, called unmodified from
  `kirby_matlab_code/` every 20 ms. Each outputs 0–1 per channel.
- **Mappings** turn the two outputs into the drive bits every 50 ms. Each is its own file, swappable
  from a list in `run_emg_car.m`:
  - `map_go_brake`: A0 → FWD, A1 → SLOW.
  - `map_prop_go_brake`: the same, but how often the bit is sent ∝ contraction strength.
  - `map_steer`: A0 → LEFT, A1 → RIGHT, both → FWD.
- **Drive window:** EMG and controller plots, Kirby's sliders, an activation-level slider, and an
  **E-STOP button/spacebar** that stays stopped until RESUME. LIVE mode starts stopped.

### 2.4 Failsafe behaviour

- `nrf_tx_drive` transmits only when a byte arrives from MATLAB. If MATLAB stops, the Nano's USB is
  unplugged or the Nano resets, the STM32 hears nothing and E-stops after 200 ms (existing
  `KEY_TIMEOUT_MS`).
- If the Uno stops sending (unplugged or reset) for 0.25 s, the drive window sends E-STOP until data
  returns.
- E-STOP is sent when `run_emg_car` starts, and 5 times whenever a drive window closes. It's also sent
  5 times on any exit: quit, error or Ctrl+C.
- Each drive prints its longest gap between commands. If it's over ~150 ms, the laptop is drawing too
  slowly: raise `PLOT_PERIOD_S` in `emg_ctrl_bridge.m`.

### 2.5 Timing and resource budget

| Item | Value |
|---|---|
| Uno sampling interrupt | 2 × `analogRead` ≈ 0.23 ms of every 1 ms |
| Uno → PC | 11 B × 1000/s = 11 kB/s of 25 kB/s |
| Uno RAM / flash | 524 B of 2 KB / 3.8 KB of 32 KB |
| MATLAB parse | ≈ 3 ms per second of data |
| MATLAB controller tick | every 20 ms (fixed schedule) |
| PC → Nano | 1 byte / 50 ms (fixed schedule, averages exactly 50 ms) |
| Radio write (Nano) | ≈ 1.5 ms good packet; RF24 default retries, worst case ≈ 40 ms (still inside the 50 ms period) |

---

## Questions for me

Answered:
1. **Shield:** classic Heart & Brain SpikerShield (not the SpikerBox), on an Arduino Uno.
2. **Channel routing:** a jumper on each shield's channel-select header, bottom shield → A0 and
   top shield → A1. This matches the J2/JP9 header in the schematics.
3. **Radio:** PA+LNA module (HW-232, screw-on antenna) on an HW-200 adapter, so VCC → Nano VBUS (5V).
4. **Pass-through:** both shields pass D0–D13 through. No longer matters, since the Uno uses only A0/A1.
5. **PC side:** MATLAB, since the EMG control algorithms are already written in MATLAB, plus live graphing.
6. **Command mapping:** build all three (go/brake, proportional go/brake, steer) as swappable files so
   the team can test them on the car.

Still open:

7. **control02:** do you want Kirby's recursive-filter controller too? It needs its memory split per
   channel first (today both channels share it).
8. **DC offset:** the volts conversion assumes the shield's reference is exactly 2.5 V. Any offset
   counts as "signal" in the RMS. Calibration absorbs this, but subtracting each channel's resting
   mean would give the controllers more range. Do you want that (it changes Kirby's math slightly)?
