/*********************************/
/*
 * full_2_channel_wireless_ctrl_alg.ino - Two-channel SpikerShield EMG -> PC control algorithm -> Nano ESP32 -> nRF -> STM32.
 * Board: Arduino Uno with two Heart & Brain SpikerShields stacked (A0 = bottom shield, A1 = top shield).
 * Started from spikershield-interface-three-channel.ino.
 *
 * Data flow:
 *   (1) Timer1 interrupt samples A0 and A1 at SAMPLING_FREQ into a small ring buffer
 *   (2) loop() sends every sample to the PC over USB as one text line: "%04d %04d\r\n" (e.g. "0512 0498")
 *   (3) The PC control algorithm replies with ONE byte per decision, a bitmask:
 *         0x01 = forward, 0x02 = slow/stop, 0x04 = left, 0x08 = right, 0x10 = EMERGENCY STOP
 *       (same byte as pc_drive.py, so the STM32 runs demo_full_PC_car_ctrl() unchanged)
 *   (4) loop() sends the newest byte to the Nano ESP32 over a 1-wire UART on D2.
 *       The Nano (nrf_uart_bridge.ino) transmits it to the STM32 over the nRF24L01+.
 *
 * Why the radio isn't on this board: every Heart & Brain SpikerShield revision has an LED (no
 * resistor) from D8-D13 to GND. Those LEDs clamp the Uno's only SPI pins (D11-D13). This sketch
 * never drives D8-D13, so the shield LEDs stay off. It only uses A0, A1, D0/D1 (USB) and D2
 * (nothing on any shield revision). See docs/uno_spikershield_nrf_plan.md.
 *
 * Link to the Nano: SoftwareSerial TX on D2, 57600 8N1, frame [0xA5][cmd][cmd ^ 0xFF].
 * SoftwareSerial turns interrupts off while it sends each byte (~174 us). So each frame (~520 us) is
 * sent right after a sampling interrupt, in the ~770 us gap before the next one, and sampling is
 * never delayed. D2 is not on the analog port, so nothing switches near A0/A1.
 *
 * FAILSAFE: a frame is sent only when the PC sends a byte. If the Python script stops, USB is
 * unplugged or this board resets, the STM32 hears nothing and stops the car after 200 ms.
 *
 * Lines starting with '#' are status messages, not samples. The PC script must skip them.
 *
 * Wiring:
 *   Uno D2 --10k--+-- Nano ESP32 D0 (RX0)
 *                 |
 *                20k (or 2x10k) -- GND        (5V -> 3.33V divider; the ESP32 is NOT 5V tolerant)
 *   Uno GND ------------ Nano GND
 *   D5 is SoftwareSerial's (unused) RX pin: leave it unconnected.
 *
 * Arduino IDE: Board = "Arduino Uno". No extra libraries (SoftwareSerial is built in).
 * Close the Serial Monitor before running the Python script (only one program can hold the port).
 */
#include <SoftwareSerial.h>

#define SAMPLING_FREQ 1000UL // [Hz]
#define SERIAL_BUFFER_SIZE 12 // "1023 1023" plus null terminator

// These values must match the PC control-algorithm script.
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

// Link to the Nano ESP32 (must match nrf_uart_bridge.ino)
#define LINK_TX_PIN 2    // free on every Heart & Brain SpikerShield revision
#define LINK_RX_PIN 5    // required by SoftwareSerial but never used (free on every revision too)
#define LINK_BAUD 57600
#define LINK_SYNC 0xA5

// Command bits, must match KEY_xxx in stm_car_proj/Demos/full_drive_demo.c
#define KEY_ESTOP 0x10

// Status line ("# link_tx=.. overflow=..") every STATUS_PERIOD_MS. 0 = never.
#define STATUS_PERIOD_MS 1000UL
/*********************************/

SoftwareSerial link(LINK_RX_PIN, LINK_TX_PIN);

volatile uint16_t ring[RING_SIZE][N_CHANS];
volatile uint8_t ring_head = 0;        // written only by the interrupt
uint8_t ring_tail = 0;                 // written only by loop()
volatile uint16_t overflow_count = 0;  // samples lost because loop() fell behind
volatile uint8_t sample_tick = 0;      // +1 every interrupt, even when a sample is dropped

unsigned long link_tx_count = 0;
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
  } else {
    ring[ring_head][0] = analogRead(emg_pins[0]);
    ring[ring_head][1] = analogRead(emg_pins[1]);
    ring_head = next;
  }
  sample_tick++;
}

// Sends one frame to the Nano. Waits (< 1 ms) for the next sampling interrupt to finish first,
// so the frame fits in the gap before the following one. Needs Timer1 running.
static void send_frame(uint8_t cmd) {
  uint8_t tick = sample_tick;
  while (sample_tick == tick) {
  }
  link.write(LINK_SYNC);
  link.write(cmd);
  link.write((uint8_t)(cmd ^ 0xFF));
  link_tx_count++;
}

void setup() {
  Serial.begin(PC_BAUD_RATE);
  link.begin(LINK_BAUD);
  link.stopListening(); // TX only: no pin-change interrupt on the unused RX pin
  delay(10);

  Serial.println("# READY 2ch EMG, 1000 Hz, '%04d %04d'. Send 1 command byte per decision.");
  timer1_start();

  send_frame(KEY_ESTOP); // start in a known stopped state
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
    send_frame(cmd);
  }

#if STATUS_PERIOD_MS > 0
  unsigned long now = millis();
  if (now - last_status_ms >= STATUS_PERIOD_MS) {
    last_status_ms = now;
    noInterrupts();
    uint16_t overflows = overflow_count;
    interrupts();
    Serial.print("# link_tx=");
    Serial.print(link_tx_count);
    Serial.print(" overflow=");
    Serial.println(overflows);
  }
#endif
}
