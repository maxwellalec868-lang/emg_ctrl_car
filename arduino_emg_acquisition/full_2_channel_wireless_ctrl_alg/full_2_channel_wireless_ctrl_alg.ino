/*********************************/
/*
 * full_2_channel_wireless_ctrl_alg.ino - Two-channel SpikerShield EMG -> PC control algorithm -> nRF -> STM32.
 * Board: Arduino Uno with two Heart & Brain SpikerShields stacked (A0 = bottom shield, A1 = top shield).
 * Started from spikershield-interface-three-channel.ino.
 *
 * Data flow:
 *   (1) Timer1 interrupt samples A0 and A1 at SAMPLING_FREQ into a small ring buffer
 *   (2) loop() sends every sample to the PC over USB as one text line: "%04d %04d\r\n" (e.g. "0512 0498")
 *   (3) The PC control algorithm (emg_ctrl_bridge.py) replies with ONE byte per decision, a bitmask:
 *         0x01 = forward, 0x02 = slow/stop, 0x04 = left, 0x08 = right, 0x10 = EMERGENCY STOP
 *       (same byte as pc_drive.py, so the STM32 runs demo_full_PC_car_ctrl() unchanged)
 *   (4) loop() forwards the newest byte to the STM32 over the nRF24L01+ as a 1-byte packet
 *
 * Why a timer interrupt instead of the original micros() busy-wait: radio.write() blocks for
 * ~1.5 ms on a good packet and up to ~13 ms when it has to retry. In the busy-wait loop that
 * would drop samples and wreck the 1 kHz timing. The interrupt keeps sampling on time and the
 * ring buffer holds RING_SIZE ms of samples while the radio is busy.
 *
 * FAILSAFE: this sketch only transmits when the PC sends a byte. If the Python script stops,
 * USB is unplugged or this board resets, the STM32 hears nothing and stops the car after 200 ms.
 *
 * Lines starting with '#' are status messages, not samples. The PC script must skip them.
 *
 * Wiring (see docs/uno_spikershield_nrf_plan.md):
 *   nRF VCC -> 3.3V   GND -> GND   CE -> D7   CSN -> D8
 *   SCK -> D13 (ICSP 3)   MOSI -> D11 (ICSP 4)   MISO -> D12 (ICSP 1)   IRQ -> not connected
 *
 * Radio settings must match the STM32: address "JOY01", channel 76, 250 kbps, PA LOW,
 * RF24 defaults for the rest (32-byte static payload, auto-ACK, CRC16).
 *
 * Arduino IDE: Board = "Arduino Uno". Library: "RF24" by TMRh20.
 * Close the Serial Monitor before running the Python script (only one program can hold the port).
 */
#include <SPI.h>
#include <RF24.h>

#define SAMPLING_FREQ 1000UL // [Hz]
#define SERIAL_BUFFER_SIZE 12 // "1023 1023" plus null terminator

// These values must match emg_ctrl_bridge.py.
#define PC_BAUD_RATE 250000   // exact on a 16 MHz Uno (115200 is 2% off)
#define N_CHANS 2
#define PC_MESSAGE_FORMAT "%04d %04d"

// Channel order: bottom/original shield, top/second shield.
const int emg_pins[N_CHANS] = { A0, A1 };
char serial_buffer[SERIAL_BUFFER_SIZE];

// Timer1: 16 MHz / 8 prescaler = 2 MHz. OCR1A = 2 MHz / SAMPLING_FREQ - 1 (1999 for 1 kHz)
#define TIMER1_OCR (2000000UL / SAMPLING_FREQ - 1)

// Ring buffer between the sampling interrupt and loop(). Must be a power of 2 and <= 256.
// 64 samples = 64 ms of slack at 1 kHz (256 bytes of the Uno's 2 KB RAM).
#define RING_SIZE 64
#define RING_MASK (RING_SIZE - 1)

// Radio: must match the STM32 (full_drive_demo.c)
#define NRF_CE_PIN 7
#define NRF_CSN_PIN 8
#define RADIO_CHANNEL 76
const byte radio_address[6] = "JOY01";

// Retries: delay 2 = 750 us between tries, 5 retries -> worst case ~13 ms blocked (fits in the ring buffer)
#define RADIO_RETRY_DELAY 2
#define RADIO_RETRY_COUNT 5

// Command bits, must match KEY_xxx in stm_car_proj/Demos/full_drive_demo.c
#define KEY_ESTOP 0x10

// Status line ("# tx_ok=.. tx_fail=.. overflow=..") every STATUS_PERIOD_MS. 0 = never.
#define STATUS_PERIOD_MS 1000UL
/*********************************/

RF24 radio(NRF_CE_PIN, NRF_CSN_PIN);

volatile uint16_t ring[RING_SIZE][N_CHANS];
volatile uint8_t ring_head = 0;        // written only by the interrupt
uint8_t ring_tail = 0;                 // written only by loop()
volatile uint16_t overflow_count = 0;  // samples lost because loop() fell behind

unsigned long tx_ok_count = 0;
unsigned long tx_fail_count = 0;
unsigned long last_status_ms = 0;

static void timer1_start() {
  cli();
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1 = 0;
  OCR1A = TIMER1_OCR;
  TCCR1B |= (1 << WGM12); // CTC mode
  TCCR1B |= (1 << CS11);  // prescaler 8
  TIMSK1 |= (1 << OCIE1A);
  sei();
}

// Runs every 1/SAMPLING_FREQ s. Two analogRead() calls take ~0.23 ms of the 1 ms period.
ISR(TIMER1_COMPA_vect) {
  uint8_t next = (ring_head + 1) & RING_MASK;
  if (next == ring_tail) { // full: drop this sample rather than overwrite unread ones
    overflow_count++;
    return;
  }
  ring[ring_head][0] = analogRead(emg_pins[0]);
  ring[ring_head][1] = analogRead(emg_pins[1]);
  ring_head = next;
}

static bool send_cmd(uint8_t cmd) {
  bool acked = radio.write(&cmd, 1); // RF24 pads to the 32-byte static payload the STM32 expects
  if (acked) tx_ok_count++; else tx_fail_count++;
  return acked;
}

void setup() {
  Serial.begin(PC_BAUD_RATE);
  delay(10);

  if (!radio.begin() || !radio.isChipConnected()) {
    while (true) { // no sampling without a radio: the PC sees this message and nothing else
      Serial.println("# ERROR nRF24 not detected. Check wiring and 3.3V power.");
      delay(1000);
    }
  }
  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_250KBPS);
  radio.setChannel(RADIO_CHANNEL);
  radio.setRetries(RADIO_RETRY_DELAY, RADIO_RETRY_COUNT);
  radio.openWritingPipe(radio_address);
  radio.stopListening(); // transmitter mode

  send_cmd(KEY_ESTOP); // start in a known stopped state

  Serial.println("# READY 2ch EMG, 1000 Hz, '%04d %04d'. Send 1 command byte per decision.");
  timer1_start();
}

void loop() {
  // (2) Stream every buffered sample to the PC
  while (ring_tail != ring_head) {
    snprintf(serial_buffer, sizeof(serial_buffer), PC_MESSAGE_FORMAT,
             ring[ring_tail][0], ring[ring_tail][1]);
    ring_tail = (ring_tail + 1) & RING_MASK;
    Serial.println(serial_buffer);
  }

  // (3) + (4) Forward the newest command byte. If several queued up, send only the newest,
  // but never lose an emergency stop that was in the queue.
  if (Serial.available()) {
    uint8_t cmd = 0;
    bool estop_seen = false;
    while (Serial.available()) {
      cmd = Serial.read();
      if (cmd & KEY_ESTOP) estop_seen = true;
    }
    if (estop_seen) cmd |= KEY_ESTOP;
    send_cmd(cmd);
  }

#if STATUS_PERIOD_MS > 0
  unsigned long now = millis();
  if (now - last_status_ms >= STATUS_PERIOD_MS) {
    last_status_ms = now;
    noInterrupts();
    uint16_t overflows = overflow_count;
    interrupts();
    Serial.print("# tx_ok=");
    Serial.print(tx_ok_count);
    Serial.print(" tx_fail=");
    Serial.print(tx_fail_count);
    Serial.print(" overflow=");
    Serial.println(overflows);
  }
#endif
}
