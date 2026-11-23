/*********************************/
/* Three-channel SpikerShield EMG acquisition for Arduino Uno */
#define SAMPLING_FREQ 1000UL // [Hz]
#define SAMPLING_PERIOD_US (1000000UL / SAMPLING_FREQ)
#define SERIAL_BUFFER_SIZE 20 // "1023 1023 1023" plus null terminator

// These values must match liveplot_triple_emg.m.
#define MATLAB_BAUD_RATE 250000
#define N_CHANS 3
#define MATLAB_MESSAGE_FORMAT "%04d %04d %04d"

// Channel order: bottom/original shield, top/second shield, third shield.
const int emg_pins[N_CHANS] = { A0, A1, A2 };
char serial_buffer[SERIAL_BUFFER_SIZE];
/*********************************/

void setup() {
  Serial.begin(MATLAB_BAUD_RATE);
  delay(10);
}

void loop() {
  unsigned long loop_start_time = micros();

  snprintf(serial_buffer, sizeof(serial_buffer), MATLAB_MESSAGE_FORMAT,
           analogRead(emg_pins[0]), analogRead(emg_pins[1]),
           analogRead(emg_pins[2]));
  Serial.println(serial_buffer);

  while (micros() - loop_start_time < SAMPLING_PERIOD_US) {
  }
}
