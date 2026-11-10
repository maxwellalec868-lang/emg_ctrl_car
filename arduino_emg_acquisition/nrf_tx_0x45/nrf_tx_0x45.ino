/*
 * nrf_tx_0x45.ino - Arduino Nano ESP32 -> STM32 radio link test.
 *
 * Sends the single byte 0x45 every 100 ms over an nRF24L01+ radio.
 * The STM32 receiver (stm_car_proj/Core/Src/main.c) turns LD2 on when it
 * receives 0x45 as the first payload byte.
 *
 * Radio settings MUST match the STM32 side:
 *   address "JOY01", channel 76, 250 kbps, PA LOW,
 *   RF24 defaults for the rest (32-byte static payload, auto-ACK, CRC16).
 *
 * Wiring (Nano ESP32 board labels):
 *   nRF VCC -> 3V3 (NOT 5V)   nRF GND  -> GND
 *   nRF CE  -> D9             nRF CSN  -> D10
 *   nRF SCK -> D13            nRF MOSI -> D11
 *   nRF MISO-> D12            nRF IRQ  -> not connected
 *
 * Arduino IDE: Board = "Arduino Nano ESP32",
 *              Tools > Pin Numbering = "By Arduino pin (default)".
 * Library: "RF24" by TMRh20.
 */
#include <SPI.h>
#include <RF24.h>

RF24 radio(D9, D10);                    // CE, CSN
const byte address[6] = "JOY01";        // must match RADIO_ADDR on the STM32

const uint8_t payload = 0x45;           // must match EXPECTED_BYTE on the STM32
const unsigned long SEND_PERIOD_MS = 100;

// Onboard RGB LED is active LOW. Don't use LED_BUILTIN: D13 is the SPI clock.
static void ledsOff() {
  digitalWrite(LED_RED, HIGH);
  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(LED_BLUE, HIGH);
}

void setup() {
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  ledsOff();

  Serial.begin(115200);
  // Native USB serial: give the PC up to 3 s to open the port.
  unsigned long t0 = millis();
  while (!Serial && millis() - t0 < 3000) {
    delay(10);
  }

  if (!radio.begin()) {
    Serial.println("NRF24 not detected! Check wiring and 3V3 power.");
    while (true) {                      // fast red blink = radio error
      digitalWrite(LED_RED, LOW);
      delay(100);
      digitalWrite(LED_RED, HIGH);
      delay(100);
    }
  }

  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_250KBPS);
  radio.setChannel(76);
  radio.openWritingPipe(address);
  radio.stopListening();                // transmitter mode

  Serial.println("Nano ESP32 0x45 transmitter ready");
}

void loop() {
  bool acked = radio.write(&payload, sizeof(payload));

  Serial.print("Sent 0x45: ");
  Serial.println(acked ? "ACK" : "NO ACK");

  // Short flash: green = receiver ACKed, red = no ACK
  digitalWrite(acked ? LED_GREEN : LED_RED, LOW);
  delay(20);
  ledsOff();

  delay(SEND_PERIOD_MS - 20);
}
