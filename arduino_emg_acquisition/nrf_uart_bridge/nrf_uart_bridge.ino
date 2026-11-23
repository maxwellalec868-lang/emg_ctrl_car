/*
 * nrf_uart_bridge.ino - Arduino Nano ESP32: UART from the Uno -> nRF24L01+ -> STM32.
 *
 * Why this board exists: the radio can't go on the Uno, because every Heart & Brain SpikerShield
 * revision has an LED (no resistor) from D8-D13 to GND, which clamps the Uno's only SPI pins
 * (D11-D13). So the Uno sends each command byte to this Nano over a 1-wire UART, and the Nano,
 * which already has working nRF wiring, transmits it.
 *
 * Link frame (from full_2_channel_wireless_ctrl_alg.ino or uno_link_test_0x45.ino), 57600 8N1:
 *   [0xA5 sync] [cmd] [cmd ^ 0xFF]
 * Frames that fail the check are dropped. If several valid frames queued up while the radio was
 * busy, only the newest is sent, but an E-STOP bit (0x10) seen in any of them is kept.
 *
 * cmd is forwarded unchanged as a 1-byte radio packet:
 *   drive bitmask 0x01 fwd, 0x02 slow, 0x04 left, 0x08 right, 0x10 E-STOP -> STM32 demo_full_PC_car_ctrl()
 *   test byte 0x45                                                      -> STM32 demo_nrf_rx_led_run()
 * FAILSAFE: this sketch only transmits when a valid frame arrives. If the Uno, the PC script or the
 * link stops, the STM32 hears nothing and E-stops after 200 ms.
 *
 * Wiring:
 *   Uno D2 --10k--+-- Nano D0 (RX0)       (divider: Uno 5V -> 3.33V, the ESP32 is NOT 5V tolerant)
 *                 |
 *                20k (or 2x10k) -- GND
 *   Uno GND ------------ Nano GND         (required)
 *   Nano D1 (TX0): not connected
 *   nRF / HW-200 adapter: CE D9, CSN D10, SCK D13, MOSI D11, MISO D12,
 *     VCC -> VBUS (5V; the HW-200 regulates to 3.3V) or 3V3 for a bare module, GND -> GND
 *   Power the Nano from its own USB cable.
 *
 * Radio settings must match the STM32: address "JOY01", channel 76, 250 kbps, PA LOW,
 * RF24 defaults for the rest (32-byte static payload, auto-ACK, CRC16).
 *
 * Arduino IDE: Board = "Arduino Nano ESP32", Pin Numbering = "By Arduino pin (default)".
 * Library: "RF24" by TMRh20. USB Serial Monitor (115200) shows a status line once a second.
 */
#include <SPI.h>
#include <RF24.h>

#define LINK_BAUD        57600
#define LINK_SYNC        0xA5
#define KEY_ESTOP        0x10    // must match KEY_ESTOP in stm_car_proj/Demos/full_drive_demo.c

#define RADIO_CHANNEL    76
#define RADIO_RETRY_DELAY 2      // 750 us between tries
#define RADIO_RETRY_COUNT 5      // worst case ~13 ms per packet, well inside the 50 ms command period
#define STATUS_PERIOD_MS 1000UL

RF24 radio(D9, D10);                    // CE, CSN
const byte address[6] = "JOY01";        // must match RADIO_ADDR on the STM32

enum LinkState { WAIT_SYNC, WAIT_CMD, WAIT_CHECK };
LinkState link_state = WAIT_SYNC;
uint8_t pending_cmd = 0;

unsigned long frames_ok = 0, frames_bad = 0, tx_ack = 0, tx_fail = 0;
uint8_t last_cmd = 0;
unsigned long last_status_ms = 0;

// Onboard RGB LED is active LOW. Don't use LED_BUILTIN: D13 is the SPI clock.
static void ledsOff() {
  digitalWrite(LED_RED, HIGH);
  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(LED_BLUE, HIGH);
}

// Reads every byte waiting on the link. Returns true if at least one valid frame arrived,
// with the newest command in *cmd_out (E-STOP bit kept if any frame had it).
static bool read_link(uint8_t *cmd_out) {
  bool got = false;
  bool estop_seen = false;
  uint8_t newest = 0;

  while (Serial0.available()) {
    uint8_t b = Serial0.read();
    switch (link_state) {
      case WAIT_SYNC:
        if (b == LINK_SYNC) link_state = WAIT_CMD;
        break;
      case WAIT_CMD:
        pending_cmd = b;
        link_state = WAIT_CHECK;
        break;
      case WAIT_CHECK:
        if ((uint8_t)(pending_cmd ^ b) == 0xFF) {
          newest = pending_cmd;
          if (newest & KEY_ESTOP) estop_seen = true;
          got = true;
          frames_ok++;
          link_state = WAIT_SYNC;
        } else {
          frames_bad++;
          link_state = (b == LINK_SYNC) ? WAIT_CMD : WAIT_SYNC;  // resync
        }
        break;
    }
  }

  if (got) *cmd_out = estop_seen ? (newest | KEY_ESTOP) : newest;
  return got;
}

void setup() {
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  ledsOff();

  Serial.begin(115200);                 // USB: status/debug only
  Serial0.begin(LINK_BAUD);             // hardware UART0: RX0 = D0 (from the Uno), TX0 = D1 unused

  if (!radio.begin()) {
    while (true) {                      // fast red blink = radio error
      Serial.println("NRF24 not detected! Check wiring and power.");
      digitalWrite(LED_RED, LOW);
      delay(100);
      digitalWrite(LED_RED, HIGH);
      delay(100);
    }
  }

  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_250KBPS);
  radio.setChannel(RADIO_CHANNEL);
  radio.setRetries(RADIO_RETRY_DELAY, RADIO_RETRY_COUNT);
  radio.openWritingPipe(address);
  radio.stopListening();                // transmitter mode
}

void loop() {
  uint8_t cmd;
  if (read_link(&cmd)) {
    bool acked = radio.write(&cmd, 1);  // RF24 pads to the 32-byte static payload the STM32 expects
    if (acked) tx_ack++; else tx_fail++;
    last_cmd = cmd;

    // Green = STM32 ACKed, red = no ACK
    digitalWrite(LED_GREEN, acked ? LOW : HIGH);
    digitalWrite(LED_RED,   acked ? HIGH : LOW);
  }

  unsigned long now = millis();
  if (now - last_status_ms >= STATUS_PERIOD_MS) {
    last_status_ms = now;
    Serial.printf("frames ok=%lu bad=%lu  radio ack=%lu fail=%lu  last=0x%02X\n",
                  frames_ok, frames_bad, tx_ack, tx_fail, last_cmd);
  }
}
