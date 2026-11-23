/*
 * uno_nrf_tx_0x45.ino - Arduino Uno (+ SpikerShield stack) -> STM32 radio link test.
 *
 * Uno version of nrf_tx_0x45 (that one is for the Nano ESP32 and will not compile on the Uno).
 * Sends the single byte 0x45 every 100 ms over an nRF24L01+ radio.
 * STM32 side: demo_nrf_rx_led_run() in stm_car_proj/Demos/demo_nrf_rx_led.c, LD2 turns on
 * while 0x45 is being received. No new STM32 code is needed.
 *
 * Radio settings MUST match the STM32 side:
 *   address "JOY01", channel 76, 250 kbps, PA LOW,
 *   RF24 defaults for the rest (32-byte static payload, auto-ACK, CRC16).
 *
 * Wiring (Uno pin numbers; SCK/MOSI/MISO are also on the 6-pin ICSP header):
 *   nRF VCC  -> 3.3V (NOT 5V)        nRF GND  -> GND
 *   nRF CE   -> D7                   nRF CSN  -> D8
 *   nRF SCK  -> D13  (ICSP pin 3)    nRF MOSI -> D11 (ICSP pin 4)
 *   nRF MISO -> D12  (ICSP pin 1)    nRF IRQ  -> not connected
 * D10 is deliberately NOT used: some Heart & Brain boards use it for a carrier signal.
 * It still becomes an output (SPI master needs that), so don't wire anything to it.
 *
 * It also prints A0/A1 so you can check the radio doesn't add noise to the EMG channels.
 *
 * Arduino IDE: Board = "Arduino Uno". Library: "RF24" by TMRh20.
 * Serial Monitor: 115200 baud.
 */
#include <SPI.h>
#include <RF24.h>
#include <printf.h>                     // ships with RF24; needed for printPrettyDetails() on AVR

#define NRF_CE_PIN   7
#define NRF_CSN_PIN  8

RF24 radio(NRF_CE_PIN, NRF_CSN_PIN);
const byte address[6] = "JOY01";        // must match RADIO_ADDR on the STM32

const uint8_t payload = 0x45;           // must match EXPECTED_BYTE on the STM32
const unsigned long SEND_PERIOD_MS = 100;

unsigned long ack_count = 0;
unsigned long fail_count = 0;

void setup() {
  // No LED feedback: the Uno's LED is on D13, which is now the SPI clock (it will flicker, that's normal)
  Serial.begin(115200);
  printf_begin();
  delay(100);

  if (!radio.begin() || !radio.isChipConnected()) {
    while (true) {
      Serial.println("NRF24 not detected! Check wiring (MOSI/MISO swapped?) and 3.3V power.");
      delay(1000);
    }
  }

  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_250KBPS);
  radio.setChannel(76);
  radio.openWritingPipe(address);
  radio.stopListening();                // transmitter mode

  radio.printPrettyDetails();           // check: channel 76, 250 KBPS, TX address 0x31304b4f4a ("JOY01")
  Serial.println("Uno 0x45 transmitter ready");
}

void loop() {
  bool acked = radio.write(&payload, sizeof(payload));
  if (acked) ack_count++; else fail_count++;

  Serial.print("Sent 0x45: ");
  Serial.print(acked ? "ACK    " : "NO ACK ");
  Serial.print(" ok=");
  Serial.print(ack_count);
  Serial.print(" fail=");
  Serial.print(fail_count);
  Serial.print("  A0=");
  Serial.print(analogRead(A0));
  Serial.print(" A1=");
  Serial.println(analogRead(A1));

  delay(SEND_PERIOD_MS);
}
