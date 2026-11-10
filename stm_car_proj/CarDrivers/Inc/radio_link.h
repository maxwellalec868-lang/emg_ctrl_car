/**
 * radio_link.h — application-level wireless link for the car.
 *
 * Sits between main.c and the nRF24 driver:
 *   - configures the radio to match the Arduino (radio_protocol.h)
 *   - drains received packets and decodes them into a drive command
 *   - tracks link health and reports "link lost" after a timeout
 *   - sends telemetry back (ACK payloads) or arbitrary packets (nrf24_send)
 *
 * main.c never touches nrf24_* directly.
 */
#ifndef RADIO_LINK_H
#define RADIO_LINK_H

#include <stdint.h>
#include <stdbool.h>
#include "nrf24l01.h"
#include "radio_protocol.h"

/* Which packet format the car expects. JOYSTICK lets you drive the car
 * with today's Uno joystick sketch, unchanged. Switch to CAR_CMD once the
 * EMG Arduino sends car_cmd_t. */
#define RADIO_RX_JOYSTICK   0
#define RADIO_RX_CAR_CMD    1
#ifndef RADIO_RX_FORMAT
#define RADIO_RX_FORMAT     RADIO_RX_JOYSTICK
#endif

/* No valid packet for this long -> failsafe. Uno sends every ~50 ms. */
#define RADIO_LINK_TIMEOUT_MS   300u

/* Joystick centre jitter, in % (only used in JOYSTICK mode). */
#define RADIO_JOY_DEADBAND_PCT  15

typedef struct {
    int8_t  throttle;      /* -100..100 % (+ forward) */
    int8_t  steering;      /* -100..100 % (- left)    */
    uint8_t seq;
    uint8_t flags;
} drive_cmd_t;

typedef struct {
    uint32_t packets_rx;
    uint32_t packets_bad;  /* wrong size / unknown type */
    uint32_t seq_gaps;     /* missed packets inferred from seq */
} radio_link_stats_t;

nrf24_status_t radio_link_init(void);
void           radio_link_poll(void);                    /* every main-loop pass */
bool           radio_link_get_command(drive_cmd_t *out); /* false = link lost    */
uint32_t       radio_link_ms_since_rx(void);
const radio_link_stats_t *radio_link_stats(void);

/* Car -> Arduino. With RADIO_USE_ACK_PAYLOADS=1 this rides on the next
 * ACK (no mode switch, no lost commands). Otherwise it's a no-op. */
void radio_link_set_telemetry(int16_t wheel_speed, uint8_t status);

/* Car -> Arduino as a normal packet to RADIO_ADDR_REMOTE. Only arrives if
 * the Arduino is listening at that moment (startListening + a reading pipe
 * on RADIO_ADDR_REMOTE). Blocks up to ~25 ms; car keeps last command. */
nrf24_tx_result_t radio_link_send(const void *data, uint8_t len);

#endif /* RADIO_LINK_H */
