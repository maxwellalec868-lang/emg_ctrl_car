/*
 * nrf_tx_cmd.ino - Arduino Nano ESP32 -> STM32 drive commands over the nRF24L01+.
 *
 * Type a letter in the Serial Monitor (115200 baud) and press Enter:
 *   f = FORWARD (speed up by one step)
 *   s = SLOW    (slow down by one step, the car stops at 0, never reverses)
 *   x = STOP    (stop immediately)
 * Several letters at once are sent one after another, e.g. "fff" = three steps up.
 *
 * Receiver: demo_emdm_radio_run() in stm_car_proj/Demos/demo_emdm.c.
 * Packet (the rest of the 32-byte payload is zero-padded by the RF24 library):
 *   byte 0 = command (CMD_xxx below)
 *   byte 1 = step size, 0 = use the STM32 default (DEMO_STEP = 5)
 *
 * NO FAILSAFE YET: the car keeps its last speed if this board resets or the link drops
 * (docs/future_work.md #2). A STOP is sent once at startup so the car begins stopped.
 *
 * Radio settings and wiring: same as nrf_tx_0x45 (see ../README.md).
 *   address "JOY01", channel 76, 250 kbps, PA LOW, RF24 defaults for the rest.
 * Library: "RF24" by TMRh20.
 */
#include <SPI.h>
#include <RF24.h>

RF24 radio(D9, D10);                    // CE, CSN
const byte address[6] = "JOY01";        // must match RADIO_ADDR on the STM32

// Must match the CMD_xxx defines in stm_car_proj/Demos/demo_emdm.c
const uint8_t CMD_FORWARD = 0x01;
const uint8_t CMD_SLOW    = 0x02;
const uint8_t CMD_STOP    = 0x03;

const uint8_t STEP = 0;                 // 0 = STM32 default step (5). Max useful value is 30

// Onboard RGB LED is active LOW. Don't use LED_BUILTIN: D13 is the SPI clock.
static void ledsOff() {
  digitalWrite(LED_RED, HIGH);
  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(LED_BLUE, HIGH);
}

// Sends [cmd, STEP] and prints/flashes whether the STM32 ACKed it
static void sendCommand(uint8_t cmd, const char* name) {
  uint8_t packet[2] = {cmd, STEP};
  bool acked = radio.write(packet, sizeof(packet));  // RF24 retries automatically before giving up

  Serial.print("Sent ");
  Serial.print(name);
  Serial.println(acked ? ": ACK" : ": NO ACK");

  // Short flash: green = receiver ACKed, red = no ACK
  digitalWrite(acked ? LED_GREEN : LED_RED, LOW);
  delay(20);
  ledsOff();
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

  Serial.println("Command sender ready. Type f (forward), s (slow), x (stop) and press Enter.");
  sendCommand(CMD_STOP, "STOP (startup)");
}

void loop() {
  if (!Serial.available()) {
    return;
  }

  char c = Serial.read();
  switch (c) {
    case 'f': case 'F': sendCommand(CMD_FORWARD, "FORWARD"); break;
    case 's': case 'S': sendCommand(CMD_SLOW,    "SLOW");    break;
    case 'x': case 'X': sendCommand(CMD_STOP,    "STOP");    break;
    case '\r': case '\n': case ' ': break;   // ignore line endings / spaces
    default:
      Serial.print("Unknown key '");
      Serial.print(c);
      Serial.println("'. Use f, s or x.");
      break;
  }
}
