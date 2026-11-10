# arduino_emg_acquisition

Arduino code for the **Arduino Nano ESP32** (ESP32-S3) side of the EMG car.

| Sketch | What it does |
|---|---|
| [nrf_tx_0x45](nrf_tx_0x45/nrf_tx_0x45.ino) | Radio link test: sends `0x45` every 100 ms to the STM32 (LD2 turns on when it's received) |

## Radio settings (must match `stm_car_proj/Core/Src/main.c`)
Address `"JOY01"`, channel 76, 250 kbps, PA LOW. Everything else uses the RF24 defaults
(32-byte payload, auto-ACK, CRC16). Both radios must be **nRF24L01+** modules.

## Wiring
| nRF24L01 | Nano ESP32 |
|---|---|
| VCC | **3V3** (never 5V/VBUS) |
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
