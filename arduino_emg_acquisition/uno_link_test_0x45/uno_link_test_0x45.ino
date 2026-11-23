/*
 * uno_link_test_0x45.ino - End-to-end link test: Uno (+ SpikerShields) -> Nano ESP32 -> nRF -> STM32.
 *
 * Sends the test byte 0x45 every 100 ms to the Nano ESP32 over the 1-wire UART on D2, using the same
 * frame as the full sketch: [0xA5][0x45][0x45 ^ 0xFF]. The Nano (nrf_uart_bridge.ino) forwards it
 * over the radio, and the STM32 running demo_nrf_rx_led_run() (stm_car_proj/Demos/demo_nrf_rx_led.c)
 * turns LD2 on while it receives 0x45. No new STM32 code is needed.
 *
 * Expected:
 *   - Nano USB Serial Monitor: "frames ok=..." climbing ~10/s, "radio ack=..." climbing, last=0x45
 *   - Nano RGB LED green, STM32 LD2 on
 *   - Unplug the D2 wire: LD2 goes off (proves the data really comes from the Uno)
 *
 * Also prints A0/A1 so you can compare EMG readings with the Nano/radio powered on and off.
 *
 * Wiring:
 *   Uno D2 --10k--+-- Nano ESP32 D0 (RX0)
 *                 |
 *                20k (or 2x10k) -- GND        (5V -> 3.33V divider; the ESP32 is NOT 5V tolerant)
 *   Uno GND ------------ Nano GND
 *   D5 is SoftwareSerial's (unused) RX pin: leave it unconnected.
 *
 * Arduino IDE: Board = "Arduino Uno". No extra libraries. Serial Monitor: 115200 baud.
 */
#include <SoftwareSerial.h>

#define LINK_TX_PIN 2
#define LINK_RX_PIN 5    // required by SoftwareSerial but never used
#define LINK_BAUD 57600  // must match nrf_uart_bridge.ino
#define LINK_SYNC 0xA5

const uint8_t TEST_BYTE = 0x45;          // must match EXPECTED_BYTE on the STM32
const unsigned long SEND_PERIOD_MS = 100;

SoftwareSerial link(LINK_RX_PIN, LINK_TX_PIN);
unsigned long frames_sent = 0;

void setup() {
  Serial.begin(115200);
  link.begin(LINK_BAUD);
  link.stopListening(); // TX only
  Serial.println("Uno link test: sending 0x45 frames to the Nano on D2");
}

void loop() {
  link.write(LINK_SYNC);
  link.write(TEST_BYTE);
  link.write((uint8_t)(TEST_BYTE ^ 0xFF));
  frames_sent++;

  Serial.print("Sent frame #");
  Serial.print(frames_sent);
  Serial.print("  A0=");
  Serial.print(analogRead(A0));
  Serial.print(" A1=");
  Serial.println(analogRead(A1));

  delay(SEND_PERIOD_MS);
}
