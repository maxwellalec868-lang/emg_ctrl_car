# Plan: EMG from the Uno + Heart & Brain SpikerShield stack to the STM32 car

Goal: the Uno (with 2 classic Heart & Brain SpikerShields stacked) samples EMG on A0/A1 and streams it
to the PC over USB. It receives the control algorithm's 1-byte command back over USB, and the command
reaches the STM32 over the nRF24L01+. The STM32 side is **unchanged**: it runs
`demo_full_PC_car_ctrl()` (`stm_car_proj/Demos/full_drive_demo.c`), which takes the same byte
`pc_drive.py` sends.

**The radio is not on the Uno.** It stays on the Nano ESP32, and the Uno sends it each command over a
1-wire serial link. Section 1.1 explains why.

```
 Shield 1 ─A0─┐
              ├─ Uno ──USB──► PC control alg ──USB──► Uno ──D2 UART──► Nano ESP32 ──nRF──► STM32
 Shield 2 ─A1─┘   (1 kHz samples up, 1 command byte / 50 ms down)                   (speed + steering)
```

| File | Board | Purpose |
|---|---|---|
| `arduino_emg_acquisition/uno_link_test_0x45/` | Uno | Part 1 end-to-end link test (sends `0x45`) |
| `arduino_emg_acquisition/nrf_uart_bridge/` | Nano ESP32 | Receives commands from the Uno, transmits them over the nRF |
| `arduino_emg_acquisition/full_2_channel_wireless_ctrl_alg/` | Uno | Part 2 full sketch |
| `arduino_emg_acquisition/full_2_channel_wireless_ctrl_alg/emg_ctrl_bridge.py` | PC | Optional test harness for the PC side (use your own control script for real runs) |

All three sketches compile (`arduino:avr:uno` and `arduino:esp32:nano_nora`, RF24 1.6.2) with no
warnings. The Nano's frame parser passed a PC unit test (clean, corrupted, split and misaligned
streams). **None of it has run on hardware yet.**

---

## Part 1: Pins and wiring

### 1.1 What the SpikerShields put on the Uno's pins

Checked against all four official schematics of the classic Heart & Brain SpikerShield (v1.0 Nov
2014, v1.1, v1.3 Jun 2015, v1.51 SMT; `backyardbrains.com/products/files/`) and every sketch in
`BackyardBrains/SpikerShield/Heart-And-Brain`:

| Uno pin | On every shield revision | Notes |
|---|---|---|
| A0–A5 | Channel-select jumper (J2/JP9): the shield's output goes to whichever pin is jumpered | Yours: bottom shield on A0, top on A1 |
| D0, D1 | Nothing on the shield (USB serial to the PC) | Never use for anything else |
| **D2** | **Nothing** | **Used for the link to the Nano** |
| D3 | Relay coil (v1.0/v1.1 only) | Avoid |
| D4, D7 | Buttons: 5V → button → pin, 10 kΩ pull-down | Avoid. Driving these LOW and pressing a button shorts 5V to the pin |
| **D5** | **Nothing** | SoftwareSerial's unused RX pin (no wire) |
| D6 | Speaker, pin → speaker → GND | Avoid |
| D8–D13 | **One LED per pin, straight to GND, no resistor** (LED1–LED6) | Avoid. See 1.2 |
| 3.3V | Nothing | |

No Backyard Brains Heart & Brain firmware uses the LEDs, buttons or speaker. The `HeartAndBrainSpikerBoxUno`
firmware's "carrier on D10" is for the standalone SpikerBox, which has its own ATmega328PB and an AM
modulator. It doesn't apply to this shield. BYB's Uno sketch for the shield
(`SpikeRecorderSpikerShield_V1_1`) reads A0–A5 and pulses D5 as an event marker, nothing else.

### 1.2 Why the radio can't go on the Uno

The nRF24L01+ talks over SPI, and the Uno's hardware SPI exists **only** on D11 (MOSI), D12 (MISO) and
D13 (SCK). The ICSP header is the same copper, not a separate set of pins. Each of those pins has a shield
LED going straight to GND with no resistor, on both of your shields:

- An LED with no resistor holds its pin at about its forward voltage, roughly 2 V.
- **SCK and MOSI:** the Uno tries to drive 5 V, but the LEDs hold the line near 2 V. The nRF needs at
  least 2.31 V (0.7 × 3.3 V) to read a HIGH, so clock and data bits get lost. The pin also has to source
  well past the ATmega's 40 mA absolute maximum.
- **MISO:** the nRF answers with a weak 3.3 V signal. The LEDs pull it to about 1.7 V. The Uno needs
  3.0 V to read a HIGH, so it reads all zeros and the radio looks "not detected".
- Bit-banged SPI on the free A2–A5 pins would avoid the LEDs. But the ATmega328P datasheet warns that
  switching digital outputs on the analog port during a conversion adds noise to A0/A1.

So the Uno does no SPI at all. **This setup never drives D8–D13**, so the shield LEDs stay dark.

### 1.3 Wiring

**Uno → Nano ESP32 (the link):**

| From | To | Why |
|---|---|---|
| Uno **D2** (top shield's female header) | **10 kΩ** → junction → Nano **D0 (RX0)** | Uno sends 5 V. The ESP32 is **not** 5 V tolerant |
| Junction | **20 kΩ** (or two 10 kΩ in series) → GND | Divider: 5 V × 20/30 = **3.33 V** at the Nano. Also limits backfeed into an unpowered Nano to about 0.5 mA |
| Uno GND | Nano GND | Required: the link needs a shared ground |
| Nano D1 (TX0) | not connected | One-way link |
| Uno D5 | not connected | SoftwareSerial's RX pin, unused (the sketch turns its interrupt off) |

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

**Power:** the Nano runs from its own USB cable, which also gives you a debug Serial Monitor. VBUS
carries 5 V only while USB is plugged in. The PA+LNA module draws up to about 115 mA in TX, which is fine
on USB. Keep `RF24_PA_LOW` (already set) and run the radio's GND wire straight back to the Nano.

**Backyard Brains firmware compatibility:** no BYB Heart & Brain sketch or shield revision uses D2 or D5.
If you flash BYB firmware back onto the Uno later, D2 is just an input and the Nano receives nothing.
There's no conflict either way.

### 1.4 Link format and timing

- Uno → Nano: SoftwareSerial TX on D2, **57600 baud 8N1**, 3-byte frame `[0xA5][cmd][cmd ^ 0xFF]`.
- The Nano drops any frame whose sync byte or complement doesn't match, then resyncs on the next `0xA5`.
- SoftwareSerial turns interrupts off for each byte it sends (about 174 µs). The full sketch therefore
  sends each frame right after a sampling interrupt finishes. The frame takes about 520 µs, which fits
  in the roughly 770 µs gap before the next sample, so 1 kHz sampling is never delayed. D2 is on Port D,
  not the analog port.
- If several frames queue up on the Nano (for example while a radio retry is running), it transmits only
  the newest one. Any E-STOP bit seen in the queue is kept.

### 1.5 Bring-up and test

1. **Nano:** open `nrf_uart_bridge.ino`, choose Board = **Arduino Nano ESP32**, and upload. Open the
   Serial Monitor at 115200. You should see a status line every second with `frames ok=0`.
   Fast red blinking means the radio wasn't detected (same wiring as before).
2. **STM32:** in `main.c` USER CODE BEGIN 2, enable only `demo_nrf_rx_led_run();` and flash.
3. **Uno (shields stacked):** wire the link per 1.3, upload `uno_link_test_0x45.ino` (Board = Arduino
   Uno), and open its Serial Monitor at 115200.
   - Expected: the Nano shows `frames ok` and `radio ack` climbing about 10 per second with `last=0x45`.
     The Nano's LED is green and the **STM32 LD2 is on**.
   - `frames ok` stuck at 0: check the D2 → divider → D0 wiring and the shared GND.
   - `frames bad` climbing: check the divider values and the baud rate (57600 on both).
   - Frames OK but radio failing: it's the radio side, same troubleshooting as before (see the
     README).
4. **Unplug the D2 wire:** LD2 should go off. That proves the data really comes from the Uno.
5. **Noise check:** compare the A0/A1 values the Uno prints with the Nano/radio powered on and off,
   electrodes attached.
6. Put `demo_full_PC_car_ctrl();` back in `main.c` for Part 2.

---

## Part 2: `full_2_channel_wireless_ctrl_alg`

Sketch name note: you asked for `full_2_channel_wireless_ctrl_alg.ioc`. `.ioc` is a CubeMX file, so the
Arduino sketch is `.ino`, and it lives in a folder with the same name (the Arduino IDE requires that).

### 2.1 Changes from the 3-channel sketch

| 3-channel | 2-channel |
|---|---|
| `N_CHANS 3`, pins `{A0, A1, A2}` | `N_CHANS 2`, pins `{A0, A1}` |
| `"%04d %04d %04d"`, buffer 20 | `"%04d %04d"`, buffer 12 |
| `MATLAB_BAUD_RATE 250000` | `PC_BAUD_RATE 250000` (same speed; exact on a 16 MHz Uno) |
| `micros()` busy-wait timing in `loop()` | **Timer1 interrupt** at 1 kHz + 64-sample ring buffer |
| no commands | Reads the PC's command byte and sends it to the Nano (1.4) |

**Why the timer interrupt:** sampling stays exactly on time whatever `loop()` is doing (USB output,
sending a frame). The ring buffer gives 64 ms of slack if the PC is slow to read.

### 2.2 Serial contract (what your Python script must do)

**Uno → PC** (250000 baud, 8N1):
- One line per sample: `"AAAA BBBB\r\n"`, where AAAA = A0 and BBBB = A1, both zero-padded 0–1023.
  1000 lines/s, about 44 % of the link.
- Lines starting with `#` are status messages, not samples: `# READY ...` at boot, then once a second
  `# link_tx=N overflow=N`. **Skip any line starting with `#`.** Set `STATUS_PERIOD_MS 0` to turn the
  status lines off.
- Opening the port **resets the Uno** (DTR). Wait about 2 s after opening, then flush the input buffer.

**PC → Uno:**
- Send **one raw byte** per decision (not text), the same bitmask as `pc_drive.py`:
  `0x01` forward, `0x02` slow, `0x04` left, `0x08` right, `0x10` E-STOP.
- Send one roughly every **50 ms** (the STM32 control tick). Never let the gap reach **200 ms**, or the STM32 failsafe
  E-stops the car.
- Send one byte at a time, not bursts. If several arrive together, the Uno forwards only the newest, but
  keeps any E-STOP bit.
- On exit or crash, send E-STOP a few times, like `pc_drive.py` does.

`emg_ctrl_bridge.py` is a minimal reference that follows this contract. Running it with `--dry-run`
proves the whole chain while only ever sending E-STOP, so the wheels can't move.

### 2.3 Radio settings (on the Nano)

Same as the STM32: `"JOY01"`, channel 76, 250 kbps, PA LOW, 32-byte static payload, auto-ACK, CRC16.
TX-side `setRetries(2, 5)` (750 µs × 5 retries, worst case about 13 ms) keeps each packet well inside the
50 ms command period. Retries only affect the transmitter, so the STM32 doesn't need any change.

### 2.4 Failsafe behaviour

- The Uno sends a frame **only** when the PC sends a byte, and the Nano transmits only when a valid
  frame arrives. If the script, the Uno, the link or the Nano stops, the STM32 stops receiving and
  E-stops after 200 ms (existing `KEY_TIMEOUT_MS`).
- At boot the Uno sends one E-STOP frame.

### 2.5 Timing and resource budget

| Item | Value |
|---|---|
| Sampling interrupt | 2 × `analogRead` ≈ 0.23 ms of every 1 ms |
| USB uplink | 11 B × 1000/s = 11 kB/s of 25 kB/s |
| Link frame to the Nano | ≈ 0.52 ms, sent in the gap after a sample (no sampling delay) |
| Radio write (Nano) | ≈ 1.5 ms good packet, ≈ 13 ms worst case |
| Ring buffer slack (Uno) | 64 ms |
| Uno RAM / flash | 690 B of 2 KB / 5.8 KB of 32 KB |

If `overflow` in the status line ever goes above 0, samples were dropped because the PC isn't reading
fast enough.

### 2.6 Bring-up order

1. Part 1 passes with both shields stacked.
2. Flash `full_2_channel_wireless_ctrl_alg.ino` to the Uno. Open the Serial Monitor at **250000** for a
   moment: you should see `# READY`, a stream of `0512 0498` lines, and `# link_tx=1 ...` (the boot E-STOP).
   **Close it.**
3. STM32 running `demo_full_PC_car_ctrl()`, **wheels off the ground**.
4. Run `emg_ctrl_bridge.py COMx --dry-run`. Check for about 1000 samples/s and `link_tx` climbing about 20/s
   on the Uno. On the Nano monitor, `frames ok` and `radio ack` should climb with `bad` near 0, and STM32
   `cmd_rx_count` should climb too, with the car staying stopped.
5. Switch to your own control-algorithm script, following 2.2. Run with the wheels still off the ground
   first and watch `keys`, `steer_deg` and `emdm.EMDM_DRIVE_SPEED` in Live Expressions.

---

## Questions for me

Answered:
1. **Shield:** classic Heart & Brain SpikerShield (not the SpikerBox), on an Arduino Uno.
2. **Channel routing:** a jumper on each shield's channel-select header, bottom shield → A0 and
   top shield → A1. This matches the J2/JP9 header in the schematics.
3. **Radio:** PA+LNA module (HW-232, screw-on antenna) on an HW-200 adapter, so VCC is 5V.
4. **Pass-through:** both shields pass D0–D13 through on female headers. D2 is wired from the top.

Still open:

5. **Sample rate:** is 1 kHz still what your algorithm expects? The sketch can do 2 kHz (change
   `SAMPLING_FREQ`), but the USB line format would then use about 88 % of the link. Above that, it would need a
   binary format. At 2 kHz, the link frame (1.4) would also have to move to 115200 baud to still fit
   between samples.
6. **Line format:** does your Python script already parse `"%04d %04d"` text lines, or does it expect
   something else (binary frames, comma-separated, a header)? It also needs to skip the `#` status lines
   (or set `STATUS_PERIOD_MS 0`).
7. **Command rate:** does your script send a byte every ~50 ms continuously (needed to stay inside the
   200 ms STM32 failsafe), or only when the decision changes? Sending only on change would trip the failsafe.
8. **MATLAB live plot:** the 3-channel sketch was paired with `liveplot_triple_emg.m`. Do you still need a
   MATLAB plot of the 2-channel stream? Only one program can hold the COM port, so the Python script would
   have to do the plotting, or forward the data on.
9. **Resistors:** do you have 10 kΩ and 20 kΩ resistors (or three 10 kΩ) for the divider? Any 1:2 pair
   from 1 kΩ:2 kΩ to 10 kΩ:20 kΩ works. Higher values limit the backfeed current better.
