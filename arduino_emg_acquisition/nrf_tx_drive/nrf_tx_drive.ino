/*
 * nrf_tx_drive.ino - Arduino Nano ESP32 -> STM32 keyboard drive bridge over the nRF24L01+.
 *
 * Forwards every byte received on USB serial (115200 baud) as a 1-byte radio packet, unchanged.
 * The byte is the key bitmask built by pc_drive.py (in this folder) every 50 ms:
 *   0x01 W = forward, 0x02 S = slow/stop, 0x04 A = left, 0x08 D = right, 0x10 Space = emergency stop
 *
 * Receiver: demo_full_PC_car_ctrl() in stm_car_proj/Demos/full_drive_demo.c.
 * FAILSAFE is on the STM32: no packet for 200 ms = emergency stop. So if this board resets,
 * the script closes or USB is unplugged, the car stops on its own.
 *
 * Close the Arduino Serial Monitor before running pc_drive.py (only one program can hold the port).
 *
 * Radio settings and wiring: same as nrf_tx_cmd / nrf_tx_0x45 (see ../README.md).
 *   address "JOY01", channel 76, 250 kbps, PA LOW, RF24 defaults for the rest.
 * Library: "RF24" by TMRh20.
 */
#include <SPI.h>
#include <RF24.h>

RF24 radio(D9, D10);                    // CE, CSN
const byte address[6] = "JOY01";        // must match RADIO_ADDR on the STM32

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

  if (!radio.begin()) {
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
}

void loop() {
  if (!Serial.available()) {
    return;
  }

  uint8_t keys = Serial.read();
  bool acked = radio.write(&keys, 1);   // RF24 retries automatically before giving up

  // Green = STM32 ACKed, red = no ACK. No delay here: packets arrive every 50 ms
  digitalWrite(LED_GREEN, acked ? LOW : HIGH);
  digitalWrite(LED_RED,   acked ? HIGH : LOW);
}
