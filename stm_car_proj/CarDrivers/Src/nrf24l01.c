/**
 * nrf24l01.c — portable nRF24L01(+) driver. See nrf24l01.h.
 *
 * Section / table numbers refer to the nRF24L01 Product Specification v2.0.
 * The nRF24L01+ (v1.0 spec) is register-compatible and adds 250 kbps.
 */
#include "nrf24l01.h"
#include "nrf24l01_regs.h"
#include "nrf24l01_port.h"
#include <string.h>

/* ---------------------------------------------------------------- state */
static struct {
    uint8_t  config_reg;      /* shadow of CONFIG                        */
    uint8_t  payload_size;    /* static payload width                    */
    uint8_t  addr_width;
    bool     dynamic;
    bool     ack_payloads;
    bool     auto_ack;
    bool     listening;
    bool     is_plus;
    uint8_t  rx_pipes;        /* EN_RXADDR bits opened by the app (1-5)  */
    volatile bool irq_flag;
} s;

/* --------------------------------------------------- low-level SPI ops */

/* Every SPI command: CSN low, command byte (STATUS comes back), data, CSN high. */
static uint8_t cmd_begin(uint8_t cmd)
{
    nrf24_port_csn(0);
    return nrf24_port_xfer(cmd);
}

static void cmd_end(void)
{
    nrf24_port_spi_idle();
    nrf24_port_csn(1);
}

static uint8_t command(uint8_t cmd)
{
    uint8_t status = cmd_begin(cmd);
    cmd_end();
    return status;
}

uint8_t nrf24_read_reg(uint8_t reg)
{
    cmd_begin(NRF_CMD_R_REGISTER | (reg & NRF_REG_ADDR_MASK));
    uint8_t v = nrf24_port_xfer(NRF_CMD_NOP);
    cmd_end();
    return v;
}

void nrf24_write_reg(uint8_t reg, uint8_t value)
{
    cmd_begin(NRF_CMD_W_REGISTER | (reg & NRF_REG_ADDR_MASK));
    nrf24_port_xfer(value);
    cmd_end();
}

/* Multi-byte register (addresses). Bytes go out in array order = LSByte first. */
static void write_reg_buf(uint8_t reg, const uint8_t *buf, uint8_t len)
{
    cmd_begin(NRF_CMD_W_REGISTER | (reg & NRF_REG_ADDR_MASK));
    for (uint8_t i = 0; i < len; i++) {
        nrf24_port_xfer(buf[i]);
    }
    cmd_end();
}

uint8_t nrf24_get_status(void)
{
    return command(NRF_CMD_NOP);
}

static void clear_irq_flags(uint8_t flags)
{
    nrf24_write_reg(NRF_REG_STATUS, flags & NRF_STATUS_IRQ_ALL); /* write-1-to-clear */
}

void nrf24_flush_rx(void) { command(NRF_CMD_FLUSH_RX); }
void nrf24_flush_tx(void) { command(NRF_CMD_FLUSH_TX); }

/* --------------------------------------------------------------- setup */

/* Enable FEATURE bits. Original nRF24L01 needs ACTIVATE (0x50,0x73) first,
 * the '+' doesn't. ACTIVATE *toggles*, so only send it if a plain write
 * didn't stick (handles MCU resets where the radio stayed powered). */
static bool set_feature(uint8_t feature)
{
    nrf24_write_reg(NRF_REG_FEATURE, feature);
    if (nrf24_read_reg(NRF_REG_FEATURE) == feature) {
        return true;
    }
    cmd_begin(NRF_CMD_ACTIVATE);
    nrf24_port_xfer(NRF_ACTIVATE_KEY);
    cmd_end();
    nrf24_write_reg(NRF_REG_FEATURE, feature);
    return nrf24_read_reg(NRF_REG_FEATURE) == feature;
}

nrf24_status_t nrf24_init(const nrf24_config_t *cfg)
{
    if (!cfg || cfg->channel > 125 || cfg->addr_width < 3 || cfg->addr_width > 5 ||
        cfg->payload_size < 1 || cfg->payload_size > NRF_MAX_PAYLOAD ||
        cfg->retry_delay > 15 || cfg->retry_count > 15 ||
        (cfg->ack_payloads && (!cfg->dynamic_payloads || !cfg->auto_ack)) ||
        (cfg->dynamic_payloads && !cfg->auto_ack)) {
        return NRF24_ERR_PARAM;
    }

    memset((void *)&s, 0, sizeof s);
    nrf24_port_init();
    nrf24_port_ce(0);
    nrf24_port_csn(1);
    nrf24_port_delay_ms(NRF_T_POR_MS);         /* power-on reset (Table 11) */

    /* Power down first: W_REGISTER is only legal in power-down/standby. */
    nrf24_write_reg(NRF_REG_CONFIG, NRF_CONFIG_EN_CRC);

    /* Presence check: write two registers and read them back. All 0x00 or
     * all 0xFF means MISO is stuck -> wiring, power or CSN problem. */
    nrf24_write_reg(NRF_REG_SETUP_AW, (uint8_t)(cfg->addr_width - 2u));
    nrf24_write_reg(NRF_REG_RF_CH, cfg->channel);
    if (nrf24_read_reg(NRF_REG_SETUP_AW) != (uint8_t)(cfg->addr_width - 2u) ||
        nrf24_read_reg(NRF_REG_RF_CH)    != cfg->channel) {
        return NRF24_ERR_NO_CHIP;
    }

    /* Detect '+' by whether the 250 kbps bit sticks (bit 5 is reserved on
     * the original part and always reads 0). */
    nrf24_write_reg(NRF_REG_RF_SETUP, NRF_RF_SETUP_RF_DR_LOW);
    s.is_plus = (nrf24_read_reg(NRF_REG_RF_SETUP) & NRF_RF_SETUP_RF_DR_LOW) != 0;
    if (cfg->data_rate == NRF24_250KBPS && !s.is_plus) {
        return NRF24_ERR_NOT_PLUS;
    }

    uint8_t rf_setup = (uint8_t)(((uint8_t)cfg->pa_level & 3u) << NRF_RF_SETUP_PWR_SHIFT)
                     | NRF_RF_SETUP_LNA_HCURR;
    if (cfg->data_rate == NRF24_2MBPS)   rf_setup |= NRF_RF_SETUP_RF_DR;
    if (cfg->data_rate == NRF24_250KBPS) rf_setup |= NRF_RF_SETUP_RF_DR_LOW;
    nrf24_write_reg(NRF_REG_RF_SETUP, rf_setup);

    /* ACK payloads need ARD >= 500 us (Table 24 note d) — 1500 us at 250 kbps */
    uint8_t ard = cfg->retry_delay;
    if (cfg->ack_payloads) {
        uint8_t min_ard = (cfg->data_rate == NRF24_250KBPS) ? 5u : 1u;
        if (ard < min_ard) ard = min_ard;
    }
    nrf24_write_reg(NRF_REG_SETUP_RETR,
                    (uint8_t)((ard << NRF_SETUP_RETR_ARD_SHIFT) | cfg->retry_count));

    nrf24_write_reg(NRF_REG_EN_AA, cfg->auto_ack ? 0x3Fu : 0x00u);
    nrf24_write_reg(NRF_REG_EN_RXADDR, 0x00u);           /* pipes opened later */

    for (uint8_t p = 0; p < 6; p++) {
        nrf24_write_reg((uint8_t)(NRF_REG_RX_PW_P0 + p), cfg->payload_size);
    }

    uint8_t feature = 0;
    if (cfg->dynamic_payloads) feature |= NRF_FEATURE_EN_DPL;
    if (cfg->ack_payloads)     feature |= NRF_FEATURE_EN_ACK_PAY;
    if (feature) {
        if (!set_feature(feature)) {
            return NRF24_ERR_FEATURE;
        }
        nrf24_write_reg(NRF_REG_DYNPD, 0x3Fu);           /* DPL on all pipes */
    } else {
        nrf24_write_reg(NRF_REG_FEATURE, 0);             /* harmless if inactive */
        nrf24_write_reg(NRF_REG_DYNPD, 0);
    }

    s.payload_size = cfg->payload_size;
    s.addr_width   = cfg->addr_width;
    s.dynamic      = cfg->dynamic_payloads;
    s.ack_payloads = cfg->ack_payloads;
    s.auto_ack     = cfg->auto_ack;

    clear_irq_flags(NRF_STATUS_IRQ_ALL);
    nrf24_flush_rx();
    nrf24_flush_tx();

    /* CRC setting + power up -> Standby-I (PRIM_RX = 0) */
    uint8_t config = NRF_CONFIG_PWR_UP;
    /* EN_CRC is forced high by the chip whenever auto-ACK is on */
    if (cfg->crc != NRF24_CRC_OFF || cfg->auto_ack) config |= NRF_CONFIG_EN_CRC;
    if (cfg->crc == NRF24_CRC_16)                   config |= NRF_CONFIG_CRCO;
    s.config_reg = config;
    nrf24_write_reg(NRF_REG_CONFIG, s.config_reg);
    nrf24_port_delay_us(NRF_T_PD2STBY_US);

    return NRF24_OK;
}

bool nrf24_is_plus(void) { return s.is_plus; }

void nrf24_power_down(void)
{
    nrf24_port_ce(0);
    s.listening = false;
    s.config_reg &= (uint8_t)~NRF_CONFIG_PWR_UP;
    nrf24_write_reg(NRF_REG_CONFIG, s.config_reg);
}

/* ------------------------------------------------------------- receive */

void nrf24_open_reading_pipe(uint8_t pipe, const uint8_t *addr)
{
    if (pipe < 1 || pipe > 5 || !addr) {
        return;
    }
    nrf24_port_ce(0);                                    /* standby for writes */
    if (pipe == 1) {
        write_reg_buf(NRF_REG_RX_ADDR_P1, addr, s.addr_width);
    } else {
        /* pipes 2-5 have only their LSByte; upper bytes come from pipe 1 */
        nrf24_write_reg((uint8_t)(NRF_REG_RX_ADDR_P0 + pipe), addr[0]);
    }
    s.rx_pipes |= (uint8_t)(1u << pipe);
    nrf24_write_reg(NRF_REG_EN_RXADDR, s.rx_pipes);
    if (s.listening) {
        nrf24_port_ce(1);
    }
}

void nrf24_start_listening(void)
{
    nrf24_port_ce(0);
    s.config_reg |= NRF_CONFIG_PWR_UP | NRF_CONFIG_PRIM_RX;
    nrf24_write_reg(NRF_REG_CONFIG, s.config_reg);
    clear_irq_flags(NRF_STATUS_IRQ_ALL);
    if (s.ack_payloads) {
        nrf24_flush_tx();            /* drop stale TX data so it isn't sent as an ACK */
    }
    nrf24_write_reg(NRF_REG_EN_RXADDR, s.rx_pipes);      /* pipe 0 off while RX */
    nrf24_port_ce(1);
    nrf24_port_delay_us(NRF_T_STBY2A_US);
    s.listening = true;
}

void nrf24_stop_listening(void)
{
    nrf24_port_ce(0);
    nrf24_port_delay_us(NRF_T_STBY2A_US);
    if (s.ack_payloads) {
        nrf24_flush_tx();
    }
    s.config_reg &= (uint8_t)~NRF_CONFIG_PRIM_RX;
    nrf24_write_reg(NRF_REG_CONFIG, s.config_reg);
    nrf24_write_reg(NRF_REG_EN_RXADDR, (uint8_t)(s.rx_pipes | 0x01u)); /* pipe 0 for ACKs */
    s.listening = false;
}

bool nrf24_available(uint8_t *pipe_out)
{
    if (nrf24_read_reg(NRF_REG_FIFO_STATUS) & NRF_FIFO_RX_EMPTY) {
        return false;
    }
    if (pipe_out) {
        *pipe_out = (uint8_t)((nrf24_get_status() & NRF_STATUS_RX_P_NO_MASK)
                              >> NRF_STATUS_RX_P_NO_SHIFT);
    }
    return true;
}

uint8_t nrf24_read(void *buf, uint8_t max_len)
{
    uint8_t width = s.payload_size;

    if (s.dynamic) {
        cmd_begin(NRF_CMD_R_RX_PL_WID);
        width = nrf24_port_xfer(NRF_CMD_NOP);
        cmd_end();
        if (width == 0 || width > NRF_MAX_PAYLOAD) {     /* corrupt: datasheet says flush */
            nrf24_flush_rx();
            clear_irq_flags(NRF_STATUS_RX_DR);
            return 0;
        }
    }

    /* Spec Table 24 note b: 1) read payload 2) clear RX_DR 3) check FIFO */
    uint8_t *out = (uint8_t *)buf;
    cmd_begin(NRF_CMD_R_RX_PAYLOAD);
    for (uint8_t i = 0; i < width; i++) {
        uint8_t b = nrf24_port_xfer(NRF_CMD_NOP);
        if (out && i < max_len) {
            out[i] = b;                  /* extra (padding) bytes are discarded */
        }
    }
    cmd_end();
    clear_irq_flags(NRF_STATUS_RX_DR);

    return (width < max_len) ? width : max_len;
}

/* ------------------------------------------------------------ transmit */

nrf24_tx_result_t nrf24_send(const uint8_t *addr, const void *buf,
                             uint8_t len, uint32_t timeout_ms)
{
    if (!addr || !buf || len == 0 || len > NRF_MAX_PAYLOAD ||
        (!s.dynamic && len > s.payload_size)) {
        return NRF24_TX_BAD_LEN;
    }

    bool was_listening = s.listening;
    if (was_listening) {
        nrf24_stop_listening();
    } else {
        nrf24_port_ce(0);
    }

    /* TX_ADDR = destination. RX_ADDR_P0 = same, so the ACK is received. */
    write_reg_buf(NRF_REG_TX_ADDR, addr, s.addr_width);
    write_reg_buf(NRF_REG_RX_ADDR_P0, addr, s.addr_width);

    clear_irq_flags(NRF_STATUS_TX_DS | NRF_STATUS_MAX_RT);

    /* Static payloads are zero-padded to payload_size (RF24 does the same). */
    const uint8_t *in = (const uint8_t *)buf;
    uint8_t width = s.dynamic ? len : s.payload_size;
    cmd_begin(NRF_CMD_W_TX_PAYLOAD);
    for (uint8_t i = 0; i < width; i++) {
        nrf24_port_xfer(i < len ? in[i] : 0u);
    }
    cmd_end();

    /* CE high starts TX (>=10 us); keep it high until the transaction ends
     * so auto-retransmits run. */
    nrf24_port_ce(1);
    nrf24_port_delay_us(NRF_T_HCE_US + 5u);

    nrf24_tx_result_t result = NRF24_TX_TIMEOUT;
    uint32_t start = nrf24_port_millis();
    for (;;) {
        uint8_t st = nrf24_get_status();
        if (st & NRF_STATUS_TX_DS) { result = NRF24_TX_OK;     break; }
        if (st & NRF_STATUS_MAX_RT){ result = NRF24_TX_MAX_RT; break; }
        if ((nrf24_port_millis() - start) > timeout_ms) {
            break;
        }
    }
    nrf24_port_ce(0);

    /* MAX_RT leaves the payload in the FIFO and blocks further TX until
     * cleared (STATUS register description). RX_DR is left alone: if the
     * ACK carried a payload, nrf24_available()/nrf24_read() will see it. */
    if (result != NRF24_TX_OK) {
        nrf24_flush_tx();
    }
    clear_irq_flags(NRF_STATUS_TX_DS | NRF_STATUS_MAX_RT);

    if (was_listening) {
        nrf24_start_listening();
    }
    return result;
}

bool nrf24_write_ack_payload(uint8_t pipe, const void *buf, uint8_t len)
{
    if (!s.ack_payloads || pipe > 5 || !buf || len == 0 || len > NRF_MAX_PAYLOAD) {
        return false;
    }
    if (nrf24_read_reg(NRF_REG_FIFO_STATUS) & NRF_FIFO_TX_FULL) {
        return false;
    }
    const uint8_t *in = (const uint8_t *)buf;
    cmd_begin((uint8_t)(NRF_CMD_W_ACK_PAYLOAD | pipe));
    for (uint8_t i = 0; i < len; i++) {
        nrf24_port_xfer(in[i]);
    }
    cmd_end();
    return true;
}

/* ------------------------------------------------------------ misc */

uint8_t nrf24_lost_packets(void)
{
    return (uint8_t)(nrf24_read_reg(NRF_REG_OBSERVE_TX) >> 4);
}

void nrf24_irq_notify(void) { s.irq_flag = true; }

bool nrf24_irq_take(void)
{
    if (s.irq_flag) {
        s.irq_flag = false;
        return true;
    }
    return false;
}
