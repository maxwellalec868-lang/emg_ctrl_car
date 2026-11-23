# Plan: nRF24L01+ on the Arduino Uno + Heart & Brain SpikerShield stack

Goal: the Uno (with 2 SpikerShields stacked) samples EMG on A0/A1, streams it to the PC over USB,
receives the control algorithm's 1-byte command back over USB, and forwards it to the STM32 over the
nRF24L01+. The STM32 side is **unchanged**: it already runs `demo_full_PC_car_ctrl()`
(`stm_car_proj/Demos/full_drive_demo.c`), which takes the same byte `pc_drive.py` sends.

Files added:

| File | Purpose |
|---|---|
| `arduino_emg_acquisition/uno_nrf_tx_0x45/uno_nrf_tx_0x45.ino` | Part 1 radio test (Uno version of `nrf_tx_0x45`) |
| `arduino_emg_acquisition/full_2_channel_wireless_ctrl_alg/full_2_channel_wireless_ctrl_alg.ino` | Part 2 full sketch |
| `arduino_emg_acquisition/full_2_channel_wireless_ctrl_alg/emg_ctrl_bridge.py` | Optional test harness for the serial link (use your own control script for real runs) |

Both sketches compile for `arduino:avr:uno` with RF24 1.6.2 (full sketch: 5.9 KB flash, 688 B RAM).
Neither has been run on hardware yet.

> The existing `nrf_tx_*` sketches target the **Nano ESP32** (`D9`, `LED_RED`, …) and will not compile
> for the Uno. That's why separate Uno sketches were needed.

---

## Part 1: Wiring the radio

### 1.1 What the SpikerShield stack already uses

From Backyard Brains' own Uno firmware for this shield (`BackyardBrains/SpikerShield`, `Heart-And-Brain/`):

| Uno pin | Used by | Notes |
|---|---|---|
| A0, A1 | EMG signal, one per shield | Your setup (A2 was the 3rd shield) |
| D0, D1 | USB serial to the PC | Never use for anything else |
| D5 | "Message impulse" event-marker output in BYB's Spike Recorder sketch | Optional; avoid it anyway |
| D10 | **Carrier output** in BYB's "Heart & Brain SpikerBox **Uno**" firmware | Only on that variant; avoid it |
| D13 | Power LED in that same variant (also the Uno's own "L" LED) | Shared with SPI SCK, see below |

Your 3-channel sketch never drives D10, and it still gets signal. That suggests your shields don't need
the carrier, but it hasn't been confirmed (see Questions).

### 1.2 Pin map

| nRF24L01+ | Uno pin (wire here, top of the stack) | ICSP equivalent (only if you can't reach D11–D13) | Why |
|---|---|---|---|
| VCC | **3.3V** (never 5V) | – | nRF supply max is 3.6 V |
| GND | GND | pin 6 | |
| CE | **D7** | – | Free on the shield; D9/D10 avoided |
| CSN | **D8** | – | Free on the shield; D10 avoided (carrier) |
| SCK | D13 | pin 3 | Hardware SPI, fixed |
| MOSI | D11 | pin 4 | Hardware SPI, fixed |
| MISO | D12 | pin 1 | Hardware SPI, fixed |
| IRQ | not connected | – | Not needed (could go to D2 later) |

Add a **10 µF (or larger) capacitor across the radio's VCC/GND**, right at the module.

**Connect SCK/MOSI/MISO to D13/D11/D12 on the top shield's header.** On the Uno these are the same
signals as ICSP pins 3/4/1, so you never need the ICSP header. The 3/4/1 in that column are ICSP pin
numbers, not Arduino pin numbers.

This works as long as both shields pass D11–D13 through (stacking headers whose pins stick out the top).
To check, stack the shields and use a multimeter in continuity mode: top shield's D13 ↔ Uno ICSP pin 3,
D11 ↔ ICSP pin 4, D12 ↔ ICSP pin 1. A beep on all three means the top of the stack carries SPI.
If a shield doesn't pass a pin through, take that signal from the layer below it or from the ICSP pin.

ICSP pinout, for reference only (2×3 header next to the ATmega328P, pin 1 has the dot):
`1 MISO · 2 5V · 3 SCK · 4 MOSI · 5 RESET · 6 GND`.

### 1.3 Pin conflicts and how they're avoided

1. **D10 (SS).** On an AVR in SPI master mode, D10 has to stay an output, or SPI can drop out of master mode.
   `SPI.begin()` sets it that way. Since some H&B boards use D10 as a carrier, CSN goes on D8 instead.
   Nothing else should be wired to D10.
2. **D13 = SCK.** The Uno's "L" LED (and any shield LED on D13) will flicker while the radio is used.
   That's harmless, but don't use `LED_BUILTIN` for status. The sketches report status over serial instead.
3. **D11/D12/D13 through the shields.** If a shield has anything on these pins (LED, button, resistor),
   it loads the SPI lines. MISO is the sensitive one because the nRF drives it weakly at 3.3 V. The test
   in 1.6 checks this: if the radio is detected and gets ACKs **with both shields stacked**, there's no conflict.
4. **Timer1.** The full sketch uses Timer1 for 1 kHz sampling. That disables `analogWrite` on D9/D10 and
   the Servo library. Nothing in this design uses those.
5. **Radio noise on the EMG.** TX bursts draw roughly 12 mA spikes that can show up on A0/A1 through a
   shared ground. The test sketch prints A0/A1 so you can compare readings with the radio on and off.
   Mitigations: the VCC cap, short radio GND wire back to the Uno's GND, and keeping the antenna away
   from the electrode cables.

### 1.4 Power and logic levels

- **Power:** the Uno R3's 3.3V pin comes from an LP2985 regulator (≈150 mA). That's fine for a plain
  nRF24L01+ module (≈12–14 mA). A **PA+LNA module** (the one with the external antenna, ≈115 mA peaks)
  should instead go on an nRF24 "socket adapter" board with its own regulator, powered from **5V**.
- **Logic:** the nRF24L01+ inputs are 5 V tolerant, so the Uno's 5 V CE/CSN/SCK/MOSI are OK. Its 3.3 V
  MISO clears the Uno's 3.0 V input-high threshold, but with little margin. This is the standard RF24 + Uno
  setup and normally works. If MISO turns out flaky, the adapter board or a level shifter fixes it.

### 1.5 Physical access with shields stacked

The shields cover the Uno headers, so connect at the **top** of the stack:

- **Option A (simplest):** plug female jumper wires onto the long stacking-header pins sticking up from
  the top shield: D7, D8, D11, D12, D13, 3.3V, GND.
- **Option B (most robust):** put a cheap proto shield on top of the stack, solder an 8-pin female
  socket for the nRF module, and add the cap. No loose jumpers next to the EMG cables.

Either way, the ICSP header is only a fallback, for when the continuity check in 1.2 shows a shield
doesn't pass D11–D13 through.

### 1.6 Bring-up and test

1. **Bare Uno first (no shields).** Wire the radio per 1.2. In Arduino IDE: Library Manager → install
   **RF24 by TMRh20** (it isn't currently installed in your IDE). Board = **Arduino Uno**. Open and upload
   `uno_nrf_tx_0x45.ino`. Serial Monitor at **115200**.
   - `NRF24 not detected` → wiring or power (MOSI/MISO swapped is the usual cause).
   - `printPrettyDetails()` should show channel 76, 250 KBPS, TX address `0x31304b4f4a` ("JOY01").
2. **STM32:** in `main.c` USER CODE BEGIN 2, enable only `demo_nrf_rx_led_run();` and flash.
   No new STM32 code is needed: this is the same receiver used with the Nano ESP32.
   - Expected: Uno prints `ACK`, and STM32 **LD2 turns on**. `rx_count` climbs in Live Expressions.
3. **Repeat with both shields stacked** and the radio wired to the top of the stack. ACKs should still
   come through. Then attach electrodes and compare A0/A1 values with the radio unplugged and plugged in.
4. Put `demo_full_PC_car_ctrl();` back in `main.c` for Part 2.

---

## Part 2: `full_2_channel_wireless_ctrl_alg`

Sketch name note: you asked for `full_2_channel_wireless_ctrl_alg.ioc`. `.ioc` is a CubeMX file, so the
Arduino sketch is `.ino`, and it lives in a folder with the same name (the Arduino IDE requires that).

### 2.1 Data flow

```
 Shield 1 ─A0─┐                                                        ┌─ EMDM (speed, I2C)
              ├─ Uno: Timer1 ISR @1 kHz → ring buffer → USB TX ─► PC control alg
 Shield 2 ─A1─┘                                                        │
                Uno: USB RX (1 byte / 50 ms) → radio.write() ─nRF─► STM32 ─┤
                                                                       └─ servo (steering, PWM)
```

### 2.2 Changes from the 3-channel sketch

| 3-channel | 2-channel |
|---|---|
| `N_CHANS 3`, pins `{A0, A1, A2}` | `N_CHANS 2`, pins `{A0, A1}` |
| `"%04d %04d %04d"`, buffer 20 | `"%04d %04d"`, buffer 12 |
| `MATLAB_BAUD_RATE 250000` | `PC_BAUD_RATE 250000` (same speed; exact on a 16 MHz Uno) |
| `micros()` busy-wait timing in `loop()` | **Timer1 interrupt** at 1 kHz + 64-sample ring buffer |
| no radio | nRF24 TX + USB RX command forwarding |

**Why the timer interrupt:** `radio.write()` blocks for about 1.5 ms when the packet is ACKed first time,
and up to about 13 ms when it has to retry. Inside the old busy-wait loop, that would skip samples and
break the 1 kHz timing your algorithm depends on. With the interrupt, sampling stays exact and the ring
buffer covers 64 ms of radio stall.

### 2.3 Serial contract (what your Python script must do)

**Uno → PC** (250000 baud, 8N1):
- One line per sample: `"AAAA BBBB\r\n"`, where AAAA = A0 and BBBB = A1, both zero-padded 0–1023.
  1000 lines/s, about 44 % of the link.
- Lines starting with `#` are status messages, not samples: `# READY ...` at boot, then once a second
  `# tx_ok=N tx_fail=N overflow=N`. **Skip any line starting with `#`.** Set `STATUS_PERIOD_MS 0`
  to turn the status lines off.
- Opening the port **resets the Uno** (DTR). Wait about 2 s after opening, then flush the input buffer.

**PC → Uno:**
- Send **one raw byte** per decision (not text), the same bitmask as `pc_drive.py`:
  `0x01` forward, `0x02` slow, `0x04` left, `0x08` right, `0x10` E-STOP.
- Send one roughly every **50 ms** (the STM32 control tick). Never let the gap reach **200 ms**, or the STM32 failsafe
  E-stops the car.
- If several bytes arrive between radio sends, the Uno forwards only the newest. If any of them had the
  E-STOP bit, that bit is kept.
- On exit or crash, send E-STOP a few times, like `pc_drive.py` does.

`emg_ctrl_bridge.py` is a minimal reference that follows this contract. Its `control_algorithm()` is just
a threshold placeholder. Running it with `--dry-run` proves the full chain (Uno receives bytes →
`tx_ok` climbs → STM32 `cmd_rx_count` climbs) while only ever sending E-STOP, so the wheels can't move.

### 2.4 Radio settings

Same as the STM32: `"JOY01"`, channel 76, 250 kbps, PA LOW, 32-byte static payload, auto-ACK, CRC16.
The only change is TX-side `setRetries(2, 5)` (750 µs × 5 retries) instead of RF24's default `(5, 15)`,
which could block for about 40 ms. Retries only affect the transmitter, so the STM32 doesn't need any change.

### 2.5 Failsafe behaviour

- The Uno transmits **only** when the PC sends a byte. If the script stops, USB is unplugged or the
  Uno resets, the STM32 stops receiving and E-stops after 200 ms (existing `KEY_TIMEOUT_MS`).
- At boot the Uno sends one E-STOP.
- If the radio isn't detected, the Uno doesn't sample at all and prints `# ERROR nRF24 not detected`
  once a second.

### 2.6 Timing and resource budget

| Item | Value |
|---|---|
| Sampling interrupt | 2 × `analogRead` ≈ 0.23 ms of every 1 ms |
| USB uplink | 11 B × 1000/s = 11 kB/s of 25 kB/s |
| Radio write | ≈1.5 ms good packet, ≈13 ms worst case (6 tries) |
| Ring buffer slack | 64 ms |
| RAM / flash | 688 B of 2 KB / 5.9 KB of 32 KB |

If `overflow` in the status line ever goes above 0, samples were dropped. That means the PC isn't reading
fast enough or the radio is retrying a lot.

### 2.7 Bring-up order

1. Part 1 passes with both shields stacked.
2. Flash `full_2_channel_wireless_ctrl_alg.ino`. Open the Serial Monitor at **250000** for a moment: you
   should see `# READY`, a stream of `0512 0498` lines, and `# tx_ok=1 ...` (the boot E-STOP). **Close it.**
3. STM32 running `demo_full_PC_car_ctrl()`, **wheels off the ground**.
4. Run `emg_ctrl_bridge.py COMx --dry-run`: check about 1000 samples/s and that `tx_ok` climbs about 20/s with
   `tx_fail` near 0. STM32 `cmd_rx_count` should climb too, with the car staying stopped.
5. Switch to your own control-algorithm script, following 2.3. Run with the wheels still off the ground
   first and watch `keys`, `steer_deg` and `emdm.EMDM_DRIVE_SPEED` in Live Expressions.

---

## Questions for me

1. **Which shield revision do you have?** Classic Heart & Brain SpikerShield, or the "Heart & Brain
   SpikerBox Uno" variant? BYB's firmware for the latter drives a carrier on D10, and that would mean D10
   needs that signal.
2. **How do the shields route to A0 vs A1?** Jumper, cut trace, or bent pin? I'm assuming each shield's
   output reaches only its own analog pin, and that neither shield touches D7, D8 or D11–D13.
3. **Which nRF module?** Plain PCB-antenna nRF24L01+ (fine on the Uno's 3.3V pin), or a PA+LNA module
   with an external antenna (needs the 5V adapter board)?
4. **Do both shields pass D11–D13 through** to the top of the stack (continuity check in 1.2), and will
   you wire with jumpers or a proto shield?
5. **Sample rate:** is 1 kHz still what your algorithm expects? The sketch can do 2 kHz (change
   `SAMPLING_FREQ`), but the USB line format would then use about 88 % of the link. Above that, it would need a
   binary format.
6. **Line format:** does your Python script already parse `"%04d %04d"` text lines, or does it expect
   something else (binary frames, comma-separated, a header)? It also needs to skip the `#` status lines
   (or set `STATUS_PERIOD_MS 0`).
7. **Command rate:** does your script send a byte every ~50 ms continuously (needed to stay inside the
   200 ms STM32 failsafe), or only when the decision changes? Sending only on change would trip the failsafe.
8. **MATLAB live plot:** the 3-channel sketch was paired with `liveplot_triple_emg.m`. Do you still need a
   MATLAB plot of the 2-channel stream? Only one program can hold the COM port, so the Python script would
   have to do the plotting, or forward the data on.
9. **Git:** `spikershield-interface-three-channel.ino` is still untracked, and the repo has many uncommitted
   changes from earlier work. Nothing has been committed for this. Do you want these new files committed
   separately?
