/*
 * demo_nrf_rx_led.c
 *
 * Radio link test (moved here from main.c unchanged).
 * Pair with arduino_emg_acquisition/nrf_tx_0x45: LD2 turns on while the
 * first byte of each received payload is 0x45.
 *
 * HAL world: includes main.h, so it must not include custom driver headers (see demos.h).
 */

#include "main.h"
#include "nrf24l01.h"
#include "demos.h"

/* Must match the Arduino sketch (RF24): openWritingPipe("JOY01"), setChannel(76) */
#define RADIO_ADDR      ((const uint8_t *)"JOY01")
#define RADIO_CHANNEL   76
#define EXPECTED_BYTE   0x45    /* first payload byte that turns LD2 on */

/* Globals so they can be watched in CubeIDE Live Expressions while debugging */
nrf24_status_t init_status;
uint32_t       rx_count;
uint8_t        last_buf[32];

void demo_nrf_rx_led_run(void)
{
  /* Defaults match RF24 radio.begin(); override what the sketch overrides. */
  nrf24_config_t cfg = NRF24_CONFIG_RF24_DEFAULTS;
  cfg.channel   = RADIO_CHANNEL;
  cfg.data_rate = NRF24_250KBPS;
  cfg.pa_level  = NRF24_PA_LOW;

  init_status = nrf24_init(&cfg);
  if (init_status != NRF24_OK)
  {
    /* Radio not responding (wiring/power) or not a '+' chip: fast blink forever */
    while (1)
    {
      HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
      HAL_Delay(100);
    }
  }

  nrf24_open_reading_pipe(1, RADIO_ADDR);
  nrf24_start_listening();

  while (1)
  {
    if (nrf24_available(NULL))
    {
      nrf24_read(last_buf, sizeof last_buf);
      rx_count++;
      HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin,
                        (last_buf[0] == EXPECTED_BYTE) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }
  }
}
