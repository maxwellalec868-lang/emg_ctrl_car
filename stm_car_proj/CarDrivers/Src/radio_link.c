/**
 * radio_link.c — see radio_link.h.
 */
#include "radio_link.h"
#include "nrf24l01_port.h"   /* millis() for the link timer */
#include "board.h"
#include <string.h>

/* Same declaration style as the Arduino: `const byte address[6] = "JOY01";`
 * (5 address bytes + the string's NUL, which is never sent). */
static const uint8_t ADDR_CAR[6]    = RADIO_ADDR_CAR;
static const uint8_t ADDR_REMOTE[6] = RADIO_ADDR_REMOTE;

static drive_cmd_t        s_cmd;
static uint32_t           s_last_rx_ms;
static bool               s_ever_rx;
#if RADIO_RX_FORMAT == RADIO_RX_CAR_CMD
static uint8_t            s_last_seq;
#endif
static radio_link_stats_t s_stats;
static car_telemetry_t    s_telem = { .type = MSG_TELEMETRY };
static uint32_t           s_last_poll_ms;

/* ---------------------------------------------------------------- init */
nrf24_status_t radio_link_init(void)
{
    nrf24_config_t cfg = NRF24_CONFIG_RF24_DEFAULTS;   /* = radio.begin()     */
    cfg.channel   = RADIO_CHANNEL;                      /* setChannel(76)      */
    cfg.data_rate = RADIO_DATA_RATE_250K ? NRF24_250KBPS : NRF24_1MBPS;
    cfg.pa_level  = (nrf24_pa_t)RADIO_PA_LEVEL;         /* setPALevel(LOW)     */
    cfg.payload_size = RADIO_PAYLOAD_SIZE;
#if RADIO_USE_ACK_PAYLOADS
    cfg.dynamic_payloads = true;                        /* enableDynamicPayloads */
    cfg.ack_payloads     = true;                        /* enableAckPayload      */
#endif

    nrf24_status_t st = nrf24_init(&cfg);
    if (st != NRF24_OK) {
        return st;
    }

    nrf24_open_reading_pipe(1, ADDR_CAR);               /* openReadingPipe(1, "JOY01") */
    nrf24_start_listening();

#if RADIO_USE_ACK_PAYLOADS
    nrf24_write_ack_payload(1, &s_telem, sizeof s_telem); /* first reply ready */
#endif
    return NRF24_OK;
}

/* -------------------------------------------------------------- decode */
static int8_t clamp_pct(int32_t v)
{
    if (v > 100)  return 100;
    if (v < -100) return -100;
    return (int8_t)v;
}

#if RADIO_RX_FORMAT == RADIO_RX_JOYSTICK
static int8_t joy_axis_to_pct(uint8_t raw, bool invert)
{
    int32_t pct = ((int32_t)raw - 128) * 100 / 127;
    if (invert) pct = -pct;
    if (pct > -RADIO_JOY_DEADBAND_PCT && pct < RADIO_JOY_DEADBAND_PCT) pct = 0;
    return clamp_pct(pct);
}
#endif

/* Returns true if the packet held a valid command. */
static bool decode(const uint8_t *p, uint8_t len)
{
#if RADIO_RX_FORMAT == RADIO_RX_JOYSTICK
    if (len < sizeof(JoystickData)) return false;
    JoystickData j;
    memcpy(&j, p, sizeof j);
    /* Same directions as the Nano LEDs: x small = up, y large = left */
    s_cmd.throttle = joy_axis_to_pct(j.x, true);   /* x low  -> +forward */
    s_cmd.steering = joy_axis_to_pct(j.y, true);   /* y high -> -left    */
    s_cmd.seq++;
    s_cmd.flags = 0;
    return true;
#else
    if (len < sizeof(car_cmd_t)) return false;
    car_cmd_t c;
    memcpy(&c, p, sizeof c);
    if (c.type != MSG_DRIVE && c.type != MSG_STOP) return false;

    if (s_ever_rx && (uint8_t)(c.seq - s_last_seq) > 1u) {
        s_stats.seq_gaps += (uint8_t)(c.seq - s_last_seq) - 1u;
    }
    s_last_seq = c.seq;

    s_cmd.throttle = (c.type == MSG_STOP) ? 0 : clamp_pct(c.throttle);
    s_cmd.steering = (c.type == MSG_STOP) ? 0 : clamp_pct(c.steering);
    s_cmd.seq      = c.seq;
    s_cmd.flags    = c.emg_flags;
    return true;
#endif
}

/* ---------------------------------------------------------------- poll */
void radio_link_poll(void)
{
    uint32_t now = nrf24_port_millis();

    /* With IRQ: only talk SPI when the chip raised an event, plus a 10 ms
     * safety poll in case an edge was missed. Without IRQ: poll each pass. */
#if NRF_USE_IRQ
    if (!nrf24_irq_take() && (now - s_last_poll_ms) < 10u) {
        return;
    }
#endif
    s_last_poll_ms = now;

    uint8_t buf[32];
    uint8_t pipe;
    /* Drain the whole RX FIFO (up to 3 packets); newest one wins. */
    while (nrf24_available(&pipe)) {
        uint8_t len = nrf24_read(buf, sizeof buf);
        if (len == 0) continue;

        s_stats.packets_rx++;
        if (decode(buf, len)) {
            s_last_rx_ms = now;
            s_ever_rx    = true;
            s_telem.ack_seq = s_cmd.seq;
#if RADIO_USE_ACK_PAYLOADS
            /* The queued ACK payload just went out with this packet's ACK;
             * queue the next one so the following ACK carries fresh data. */
            s_telem.lost_packets = nrf24_lost_packets();
            nrf24_write_ack_payload(1, &s_telem, sizeof s_telem);
#endif
        } else {
            s_stats.packets_bad++;
        }
    }
}

bool radio_link_get_command(drive_cmd_t *out)
{
    bool alive = s_ever_rx && (nrf24_port_millis() - s_last_rx_ms) <= RADIO_LINK_TIMEOUT_MS;
    if (out) {
        *out = s_cmd;
        if (!alive) {
            out->throttle = 0;
            out->steering = 0;
        }
    }
    return alive;
}

uint32_t radio_link_ms_since_rx(void)
{
    return s_ever_rx ? (nrf24_port_millis() - s_last_rx_ms) : UINT32_MAX;
}

const radio_link_stats_t *radio_link_stats(void) { return &s_stats; }

/* ------------------------------------------------------------ transmit */
void radio_link_set_telemetry(int16_t wheel_speed, uint8_t status)
{
    s_telem.wheel_speed = wheel_speed;
    s_telem.status      = status;
    /* sent automatically on the next ACK (see radio_link_poll) */
}

nrf24_tx_result_t radio_link_send(const void *data, uint8_t len)
{
    return nrf24_send(ADDR_REMOTE, data, len, 30u);
}
