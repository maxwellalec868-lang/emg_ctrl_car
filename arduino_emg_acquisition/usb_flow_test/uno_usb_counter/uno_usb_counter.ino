/*
 * uno_usb_counter.ino - USB flow test, Uno side. NOT for driving: it sends a counter, not EMG.
 *
 * Same output format, baud rate and 1 kHz timing as full_2_channel_wireless_ctrl_alg.ino, but each
 * line carries a known pattern instead of analogRead():
 *   "%04d %04d\r\n"  ->  first = counter 0..1023 (wraps), second = 1023 - counter
 * MATLAB usb_flow_test.m checks that the counter never skips (no lost lines) and that the second
 * number always matches (no corrupted lines). It also forwards bytes to the Nano (nano_usb_echo.ino).
 *
 * Lines starting with '#' are status: "# READY ..." at boot, "# overflow=N" once a second.
 *
 * Arduino IDE: Board = "Arduino Uno". Close the Serial Monitor before running usb_flow_test.m.
 * When the test is done, flash full_2_channel_wireless_ctrl_alg.ino back onto the Uno.
 */

#define SAMPLING_FREQ 1000UL
#define PC_BAUD_RATE 250000
#define TIMER1_OCR (2000000UL / SAMPLING_FREQ - 1)   // 16 MHz / 8 = 2 MHz timer clock
#define RING_SIZE 64                                 // power of 2, <= 256
#define RING_MASK (RING_SIZE - 1)
#define STATUS_PERIOD_MS 1000UL

volatile uint16_t ring[RING_SIZE][2];
volatile uint8_t ring_head = 0;
uint8_t ring_tail = 0;
volatile uint16_t overflow_count = 0;
uint16_t counter = 0;                 // only touched by the interrupt
char serial_buffer[12];
unsigned long last_status_ms = 0;

ISR(TIMER1_COMPA_vect) {
  uint8_t next = (ring_head + 1) & RING_MASK;
  if (next == ring_tail) {
    overflow_count++;                 // the counter still advances, so MATLAB sees the gap
  } else {
    ring[ring_head][0] = counter;
    ring[ring_head][1] = 1023 - counter;
    ring_head = next;
  }
  counter = (counter + 1) & 1023;
}

void setup() {
  Serial.begin(PC_BAUD_RATE);
  delay(10);
  Serial.println("# READY USB flow test: counter pattern, NOT EMG");

  cli();
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1 = 0;
  OCR1A = TIMER1_OCR;
  TCCR1B |= (1 << WGM12);             // CTC mode
  TCCR1B |= (1 << CS11);              // prescaler 8
  TIMSK1 |= (1 << OCIE1A);
  sei();
}

void loop() {
  while (ring_tail != ring_head) {
    snprintf(serial_buffer, sizeof(serial_buffer), "%04d %04d", ring[ring_tail][0], ring[ring_tail][1]);
    ring_tail = (ring_tail + 1) & RING_MASK;
    Serial.println(serial_buffer);
  }

  unsigned long now = millis();
  if (now - last_status_ms >= STATUS_PERIOD_MS) {
    last_status_ms = now;
    noInterrupts();
    uint16_t overflows = overflow_count;
    interrupts();
    Serial.print("# overflow=");
    Serial.println(overflows);
  }
}
