/**
 * nrf24l01_regs.h — SPI commands, register addresses and bit positions.
 *
 * Source: nRF24L01 Product Specification v2.0
 *   Table 16 (SPI command set, p.46-47)
 *   Table 24 (register map, p.53-58)
 */
#ifndef NRF24L01_REGS_H
#define NRF24L01_REGS_H

/* ---------------- SPI commands (Table 16) ---------------- */
#define NRF_CMD_R_REGISTER          0x00u   /* | 5-bit addr */
#define NRF_CMD_W_REGISTER          0x20u   /* | 5-bit addr; standby/power-down only */
#define NRF_CMD_R_RX_PAYLOAD        0x61u
#define NRF_CMD_W_TX_PAYLOAD        0xA0u
#define NRF_CMD_FLUSH_TX            0xE1u
#define NRF_CMD_FLUSH_RX            0xE2u
#define NRF_CMD_REUSE_TX_PL         0xE3u
#define NRF_CMD_ACTIVATE            0x50u   /* followed by 0x73 (non-'+' parts) */
#define NRF_CMD_R_RX_PL_WID         0x60u
#define NRF_CMD_W_ACK_PAYLOAD       0xA8u   /* | pipe (0-5) */
#define NRF_CMD_W_TX_PAYLOAD_NOACK  0xB0u
#define NRF_CMD_NOP                 0xFFu

#define NRF_ACTIVATE_KEY            0x73u
#define NRF_REG_ADDR_MASK           0x1Fu

/* ---------------- Registers (Table 24) ---------------- */
#define NRF_REG_CONFIG      0x00u
#define NRF_REG_EN_AA       0x01u
#define NRF_REG_EN_RXADDR   0x02u
#define NRF_REG_SETUP_AW    0x03u
#define NRF_REG_SETUP_RETR  0x04u
#define NRF_REG_RF_CH       0x05u
#define NRF_REG_RF_SETUP    0x06u
#define NRF_REG_STATUS      0x07u
#define NRF_REG_OBSERVE_TX  0x08u
#define NRF_REG_CD          0x09u   /* RPD on nRF24L01+ */
#define NRF_REG_RX_ADDR_P0  0x0Au
#define NRF_REG_RX_ADDR_P1  0x0Bu
#define NRF_REG_RX_ADDR_P2  0x0Cu
#define NRF_REG_RX_ADDR_P3  0x0Du
#define NRF_REG_RX_ADDR_P4  0x0Eu
#define NRF_REG_RX_ADDR_P5  0x0Fu
#define NRF_REG_TX_ADDR     0x10u
#define NRF_REG_RX_PW_P0    0x11u   /* P1..P5 follow at 0x12..0x16 */
#define NRF_REG_FIFO_STATUS 0x17u
/* 0x18-0x1B reserved for test — never write */
#define NRF_REG_DYNPD       0x1Cu
#define NRF_REG_FEATURE     0x1Du

/* CONFIG bits */
#define NRF_CONFIG_MASK_RX_DR   (1u << 6)
#define NRF_CONFIG_MASK_TX_DS   (1u << 5)
#define NRF_CONFIG_MASK_MAX_RT  (1u << 4)
#define NRF_CONFIG_EN_CRC       (1u << 3)
#define NRF_CONFIG_CRCO         (1u << 2)   /* 1 = 2-byte CRC */
#define NRF_CONFIG_PWR_UP       (1u << 1)
#define NRF_CONFIG_PRIM_RX      (1u << 0)

/* SETUP_RETR fields */
#define NRF_SETUP_RETR_ARD_SHIFT 4u         /* (ARD+1)*250 us */
#define NRF_SETUP_RETR_ARC_MASK  0x0Fu

/* RF_SETUP bits */
#define NRF_RF_SETUP_RF_DR_LOW  (1u << 5)   /* nRF24L01+ only: 250 kbps */
#define NRF_RF_SETUP_RF_DR      (1u << 3)   /* 0 = 1 Mbps, 1 = 2 Mbps */
#define NRF_RF_SETUP_PWR_SHIFT  1u          /* 2-bit RF_PWR */
#define NRF_RF_SETUP_LNA_HCURR  (1u << 0)

/* STATUS bits */
#define NRF_STATUS_RX_DR        (1u << 6)
#define NRF_STATUS_TX_DS        (1u << 5)
#define NRF_STATUS_MAX_RT       (1u << 4)
#define NRF_STATUS_IRQ_ALL      (NRF_STATUS_RX_DR | NRF_STATUS_TX_DS | NRF_STATUS_MAX_RT)
#define NRF_STATUS_RX_P_NO_SHIFT 1u
#define NRF_STATUS_RX_P_NO_MASK (7u << 1)   /* 7 = RX FIFO empty */
#define NRF_STATUS_TX_FULL      (1u << 0)

/* FIFO_STATUS bits */
#define NRF_FIFO_TX_REUSE       (1u << 6)
#define NRF_FIFO_TX_FULL        (1u << 5)
#define NRF_FIFO_TX_EMPTY       (1u << 4)
#define NRF_FIFO_RX_FULL        (1u << 1)
#define NRF_FIFO_RX_EMPTY       (1u << 0)

/* FEATURE bits */
#define NRF_FEATURE_EN_DPL      (1u << 2)
#define NRF_FEATURE_EN_ACK_PAY  (1u << 1)
#define NRF_FEATURE_EN_DYN_ACK  (1u << 0)

/* Timing (Table 13, Table 11) */
#define NRF_T_POR_MS            11u   /* power-on reset, max 10.3 ms  */
#define NRF_T_PD2STBY_US        1500u /* power down -> standby        */
#define NRF_T_STBY2A_US         130u  /* standby -> TX/RX settling    */
#define NRF_T_HCE_US            10u   /* minimum CE high pulse        */

#define NRF_MAX_PAYLOAD         32u

#endif /* NRF24L01_REGS_H */
