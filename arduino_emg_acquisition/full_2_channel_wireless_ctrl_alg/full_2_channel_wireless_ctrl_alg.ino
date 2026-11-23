/*********************************/
/*
 * full_2_channel_wireless_ctrl_alg.ino - Two-channel SpikerShield EMG acquisition for the wireless car.
 * Board: Arduino Uno with two Heart & Brain SpikerShields stacked (A0 = bottom shield, A1 = top shield).
 * Started from spikershield-interface-three-channel.ino.
 *
 * This board only acquires. The full system:
 *   Uno (this sketch) --USB--> MATLAB emg_ctrl_bridge.m (control algorithm + live plot)
 *                              --USB--> Nano ESP32 (nrf_tx_drive.ino) --nRF--> STM32 demo_full_PC_car_ctrl()
 * Both boards plug into the laptop; there are no wires between them.
 *
 * Why the radio isn't on this board: every Heart & Brain SpikerShield revision has an LED (no
 * resistor) from D8-D13 to GND. Those LEDs clamp the Uno's only SPI pins (D11-D13). This sketch
 * uses only A0, A1 and USB (D0/D1), so it can't conflict with anything on the shields.
 * See docs/uno_spikershield_nrf_plan.md.
 *
 * Output: one text line per sample, "%04d %04d\r\n" (A0 A1, e.g. "0512 0498"), at 250000 baud.
 * Lines starting with '#' are status messages, not samples: "# READY ..." at boot, then
 * "# overflow=N" once a second (N > 0 means samples were dropped because the PC fell behind).
 *
 * Timer1 interrupt sampling keeps the 1 kHz timing exact even when Serial.println() has to wait
 * for USB. The 64-sample ring buffer covers 64 ms of slow reading on the PC side.
 *
 * Arduino IDE: Board = "Arduino Uno". No extra libraries.
 * Close the Serial Monitor before running emg_ctrl_bridge.m (only one program can hold the port).
 */

#define SAMPLING_FREQ 1000UL // [Hz]
#define SERIAL_BUFFER_SIZE 12 // "1023 1023" plus null terminator

// These values must match emg_ctrl_bridge.m / parse_emg_chunk.m.
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

// Status line ("# overflow=..") every STATUS_PERIOD_MS. 0 = never.
#define STATUS_PERIOD_MS 1000UL
/*********************************/

volatile uint16_t ring[RING_SIZE][N_CHANS];
volatile uint8_t ring_head = 0;        // written only by the interrupt
uint8_t ring_tail = 0;                 // written only by loop()
volatile uint16_t overflow_count = 0;  // samples lost because loop() fell behind

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

void setup() {
  Serial.begin(PC_BAUD_RATE);
  delay(10);
  Serial.println("# READY 2ch EMG, 1000 Hz, '%04d %04d'");
  timer1_start();
}

void loop() {
  // Stream every buffered sample to the PC
  while (ring_tail != ring_head) {
    snprintf(serial_buffer, sizeof(serial_buffer), PC_MESSAGE_FORMAT,
             ring[ring_tail][0], ring[ring_tail][1]);
    ring_tail = (ring_tail + 1) & RING_MASK;
    Serial.println(serial_buffer);
  }

#if STATUS_PERIOD_MS > 0
  unsigned long now = millis();
  if (now - last_status_ms >= STATUS_PERIOD_MS) {
    last_status_ms = now;
    noInterrupts();
    uint16_t overflows = overflow_count;
    interrupts();
    Serial.print("# overflow=");
    Serial.println(overflows);
  }
#endif
}
