/**
 * nrf24l01.h — nRF24L01(+) driver: receive, transmit, and ACK payloads.
 *
 * Designed to interoperate with the Arduino RF24 library (TMRh20) that the
 * Uno/Nano sketches use. NRF24_CONFIG_RF24_DEFAULTS reproduces what
 * radio.begin() configures, so you only override what the sketch overrides
 * (setChannel, setDataRate, setPALevel, ...).
 *
 * Pipe usage (same convention as RF24):
 *   pipe 0   reserved: receives ACKs while this side is transmitting
 *   pipe 1-5 reading pipes opened with nrf24_open_reading_pipe()
 *
 * Address bytes are written in array order, which is what RF24 does with a
 * `const byte address[6] = "JOY01";` array, so passing the same 5 chars here
 * gives the same on-air address.
 *
 * Threading: call everything from the main loop. The IRQ handler only
 * calls nrf24_irq_notify(), which sets a flag — no SPI inside interrupts.
 */
#ifndef NRF24L01_H
#define NRF24L01_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    NRF24_1MBPS   = 0,
    NRF24_2MBPS   = 1,
    NRF24_250KBPS = 2,     /* nRF24L01+ only */
} nrf24_datarate_t;

/* Same numbering as RF24_PA_MIN..RF24_PA_MAX */
typedef enum {
    NRF24_PA_MIN  = 0,     /* -18 dBm */
    NRF24_PA_LOW  = 1,     /* -12 dBm */
    NRF24_PA_HIGH = 2,     /*  -6 dBm */
    NRF24_PA_MAX  = 3,     /*   0 dBm */
} nrf24_pa_t;

typedef enum {
    NRF24_CRC_OFF = 0,
    NRF24_CRC_8   = 1,
    NRF24_CRC_16  = 2,
} nrf24_crc_t;

typedef struct {
    uint8_t          channel;          /* 0-125: freq = 2400 + channel MHz */
    nrf24_datarate_t data_rate;
    nrf24_pa_t       pa_level;
    nrf24_crc_t      crc;              /* forced on if auto_ack            */
    uint8_t          addr_width;       /* 3, 4 or 5 bytes                  */
    uint8_t          payload_size;     /* static size 1-32 (if !dynamic)   */
    bool             auto_ack;
    bool             dynamic_payloads;
    bool             ack_payloads;     /* needs dynamic_payloads + auto_ack */
    uint8_t          retry_delay;      /* ARD 0-15: wait (n+1)*250 us      */
    uint8_t          retry_count;      /* ARC 0-15                          */
} nrf24_config_t;

/* What RF24::begin() sets up. */
#define NRF24_CONFIG_RF24_DEFAULTS {   \
    .channel          = 76,            \
    .data_rate        = NRF24_1MBPS,   \
    .pa_level         = NRF24_PA_MAX,  \
    .crc              = NRF24_CRC_16,  \
    .addr_width       = 5,             \
    .payload_size     = 32,            \
    .auto_ack         = true,          \
    .dynamic_payloads = false,         \
    .ack_payloads     = false,         \
    .retry_delay      = 5,             \
    .retry_count      = 15,            \
}

typedef enum {
    NRF24_OK = 0,
    NRF24_ERR_NO_CHIP,     /* SPI readback failed: wiring / power / CSN  */
    NRF24_ERR_PARAM,       /* invalid config value                       */
    NRF24_ERR_NOT_PLUS,    /* 250 kbps requested on a non-'+' chip       */
    NRF24_ERR_FEATURE,     /* couldn't enable dynamic / ACK payloads     */
} nrf24_status_t;

typedef enum {
    NRF24_TX_OK = 0,       /* sent (and ACKed, if auto_ack)              */
    NRF24_TX_MAX_RT,       /* no ACK after all retries                   */
    NRF24_TX_TIMEOUT,      /* chip never reported done (wiring/CE?)      */
    NRF24_TX_BAD_LEN,
} nrf24_tx_result_t;

/* ---- setup ---- */
nrf24_status_t nrf24_init(const nrf24_config_t *cfg);
bool           nrf24_is_plus(void);
void           nrf24_power_down(void);

/* ---- receive ---- */
void    nrf24_open_reading_pipe(uint8_t pipe, const uint8_t *addr); /* pipe 1-5 */
void    nrf24_start_listening(void);
void    nrf24_stop_listening(void);
bool    nrf24_available(uint8_t *pipe_out);   /* pipe_out may be NULL     */
uint8_t nrf24_read(void *buf, uint8_t max_len);/* returns payload length; 0 = none/corrupt */

/* ---- transmit ----
 * Blocking. Switches to TX, sends one packet to `addr`, waits for the
 * ACK (or retries to run out), then goes back to listening if it was
 * listening before. Worst case with ARD=5, ARC=15 is ~25 ms. */
nrf24_tx_result_t nrf24_send(const uint8_t *addr, const void *buf,
                             uint8_t len, uint32_t timeout_ms);

/* ---- ACK payloads (PRX side "reply" without switching modes) ----
 * Queue up to 3 payloads; each is attached to the ACK of the next packet
 * received on `pipe`. Returns false if TX FIFO full / feature disabled. */
bool    nrf24_write_ack_payload(uint8_t pipe, const void *buf, uint8_t len);

/* ---- housekeeping / debug ---- */
void    nrf24_flush_rx(void);
void    nrf24_flush_tx(void);
uint8_t nrf24_get_status(void);
uint8_t nrf24_lost_packets(void);             /* OBSERVE_TX.PLOS_CNT */
uint8_t nrf24_read_reg(uint8_t reg);
void    nrf24_write_reg(uint8_t reg, uint8_t value);

/* ---- IRQ glue ---- */
void    nrf24_irq_notify(void);               /* call from the EXTI ISR  */
bool    nrf24_irq_take(void);                 /* true once per IRQ edge */

#endif /* NRF24L01_H */
