# arduino_emg_acquisition

Arduino code for the **Arduino Nano ESP32** (ESP32-S3) side of the EMG car.

| Sketch | What it does |
|---|---|
| [nrf_tx_0x45](nrf_tx_0x45/nrf_tx_0x45.ino) | Radio link test: sends `0x45` every 100 ms. STM32 side: `demo_nrf_rx_led_run()` (LD2 turns on when it's received) |
| [nrf_tx_cmd](nrf_tx_cmd/nrf_tx_cmd.ino) | Drive commands: type `f` (FORWARD), `s` (SLOW) or `x` (STOP) in the Serial Monitor. STM32 side: `demo_emdm_radio_run()` |
| [nrf_tx_drive](nrf_tx_drive/nrf_tx_drive.ino) | Forwards every byte from USB serial (115200) over the nRF. Used by `pc_drive.py` (keyboard) and by the EMG setup below. STM32 side: `demo_full_PC_car_ctrl()` |

**EMG setup** (Uno with 2 Heart & Brain SpikerShields stacked + the Nano running `nrf_tx_drive`, both on
USB to the laptop, no wires between them; see [docs/uno_spikershield_nrf_plan.md](../docs/uno_spikershield_nrf_plan.md)):

| File | What it does |
|---|---|
| [full_2_channel_wireless_ctrl_alg.ino](full_2_channel_wireless_ctrl_alg/full_2_channel_wireless_ctrl_alg.ino) | **Uno:** samples A0/A1 at 1 kHz, streams `"%04d %04d"` lines over USB at 250000 baud |
| [matlab_ACQ_CA/](../matlab_ACQ_CA/README.md) | **MATLAB:** `run_emg_car("COM_uno", "COM_nano")` calibrates the user, runs Kirby's controller, maps it to drive commands and sends them to the Nano, with a live drive window and E-STOP. See its README |
| [usb_flow_test/](usb_flow_test/usb_flow_test.m) | **Test only, no radio:** `uno_usb_counter` (Uno) + `nano_usb_echo` (Nano) + `usb_flow_test.m` check that data flows Uno → MATLAB → Nano with no lost or corrupted bytes. Prints PASS/FAIL |
| [spikershield-interface-three-channel](spikershield-interface-three-channel.ino) | **Uno:** original 3-channel acquisition (A0–A2) for the MATLAB live plot |

## Radio settings (must match `stm_car_proj/Demos/`)
Address `"JOY01"`, channel 76, 250 kbps, PA LOW. Everything else uses the RF24 defaults
(32-byte payload, auto-ACK, CRC16). Both radios must be **nRF24L01+** modules.

## Wiring
| nRF24L01 | Nano ESP32 |
|---|---|
| VCC | Bare module: **3V3** (never 5V/VBUS). On an HW-200 adapter (PA+LNA module): **VBUS** (5V), the adapter regulates to 3.3V |
| GND | GND |
| CE | D9 |
| CSN | D10 |
| SCK | D13 |
| MOSI | D11 |
| MISO | D12 |
| IRQ | not connected |

A 10 µF capacitor across the radio's VCC/GND is recommended.

## Flashing with Arduino IDE 2 (one-time setup + upload)
1. Install **Arduino IDE 2** from arduino.cc with the default options.
2. **Boards Manager** (second icon on the left sidebar): search `Nano ESP32` and install
   **"Arduino ESP32 Boards" by Arduino**. Accept any driver prompts.
3. **Library Manager** (books icon): search `RF24` and install **RF24 by TMRh20**.
4. *File → Open…* → `arduino_emg_acquisition/nrf_tx_0x45/nrf_tx_0x45.ino`.
5. Plug in the board over USB-C. *Tools → Board → Arduino ESP32 Boards → **Arduino Nano ESP32***.
6. *Tools → Pin Numbering* → **By Arduino pin (default)**.
7. *Tools → Port* → the `COMx (Arduino Nano ESP32)` entry.
8. Click **✓ Verify**. It should report "Done compiling".
9. Click **→ Upload**. It should report "Done uploading".
   - Upload fails or no port? **Double-tap the reset button**. The RGB LED pulses green (bootloader
     mode). Re-select the port, upload again, then press reset once to start the sketch.
10. *Tools → Serial Monitor*, set the baud to **115200**. You should see `Sent 0x45: ACK` lines.
    If you see nothing, press reset once with the monitor open.

## What the LEDs mean
- Short **green** flash: the packet was ACKed by the STM32 radio.
- Short **red** flash: no ACK, so the receiver isn't hearing us.
- Fast continuous **red** blink: the nRF24 module wasn't detected (wiring or power).
- The orange D13 LED flickering is normal: D13 is also the radio's SPI clock.

## Troubleshooting
- **Not detected:** check MOSI/MISO aren't swapped and that VCC is on 3V3.
- **NO ACK:** the STM32 isn't receiving. Check `init_status` in CubeIDE Live Expressions and the
  STM32 radio wiring.
- **ACK but LD2 stays off:** inspect `last_buf[0]` and `rx_count` on the STM32 in the debugger.

## Using nrf_tx_cmd
Flash it the same way as above (open `nrf_tx_cmd/nrf_tx_cmd.ino` in step 4). In the Serial Monitor,
type into the **Message** box at the top and press **Enter**:
- `f`: speed up one step (default 5, capped at 30 on the STM32)
- `s`: slow down one step (stops at 0, never reverses)
- `x`: stop now
- `fff`: three steps at once

A STOP is sent once at startup. **There is no failsafe yet:** if the link drops, the car keeps
its last speed, so keep the wheels off the ground or a hand on the power switch. On the STM32,
watch `cmd_rx_count`, `last_cmd` and `emdm.EMDM_DRIVE_SPEED` in Live Expressions.
