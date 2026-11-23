/*
 * nano_usb_echo.ino - USB flow test, Nano ESP32 side. No radio is used.
 *
 * Replies to every byte received over USB with its bitwise inverse (b ^ 0xFF), e.g. 0x5A -> 0xA5.
 * MATLAB usb_flow_test.m sends bytes the same way emg_ctrl_bridge.m sends drive commands (115200,
 * one byte every 50 ms) and checks every reply. The inversion proves the reply was produced by this
 * board, not an echo somewhere else.
 *
 * LEDs (active LOW RGB): blue on = this test firmware is running; green toggles on every byte.
 *
 * Arduino IDE: Board = "Arduino Nano ESP32". No libraries needed.
 * When the test is done, flash nrf_tx_drive.ino back onto the Nano.
 */

bool green_on = false;

void setup() {
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  digitalWrite(LED_RED, HIGH);
  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(LED_BLUE, LOW);         // blue on: echo test firmware

  Serial.begin(115200);                // native USB; same port setting as nrf_tx_drive.ino
}

void loop() {
  while (Serial.available()) {
    uint8_t b = Serial.read();
    Serial.write((uint8_t)(b ^ 0xFF));
    green_on = !green_on;
    digitalWrite(LED_GREEN, green_on ? LOW : HIGH);
  }
}
