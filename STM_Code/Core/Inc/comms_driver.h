/**
 * @file    comms_driver.h / comms_driver.c
 * @brief   SPI driver for the AX.25 comms controller
 *
 * The comms controller handles AX.25 framing internally.
 * The OBC:
 *   - Sends raw data payloads to the controller for downlink
 *   - Receives telecommand payloads from the controller
 *   - Polls the controller for link status
 *
 * SPI frame format is a PLACEHOLDER. Define the exact
 * command/response protocol once the comms controller
 * firmware/datasheet is available.
 *
 * SPI mode: CPOL=0 CPHA=0 (Mode 0) -- verify with your hardware.
 * Max SPI clock: keep below 1 MHz for EM unless tested otherwise.
 */

#ifndef COMMS_DRIVER_H
#define COMMS_DRIVER_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* Maximum payload sizes -- adjust to your comms controller limits */
#define COMMS_MAX_DOWNLINK_PAYLOAD   200U
#define COMMS_MAX_UPLINK_PAYLOAD     64U

#define COMMS_SPI_HANDLE hspi1
#define COMMS_CS_PORT         GPIOA
#define COMMS_CS_PIN          GPIO_PIN_3


typedef enum {
    COMMS_OK               = 0,
    COMMS_ERR_HAL          = 1,
    COMMS_ERR_NO_DATA      = 2,
    COMMS_ERR_BUFFER_FULL  = 3,
} COMMS_Status_t;

/* Command opcodes (placeholders -- define with your comms team) */
#define COMMS_CMD_TX_DATA      0xA1
#define COMMS_CMD_RX_POLL      0xA2
#define COMMS_CMD_GET_STATUS   0xA3
#define COMMS_CMD_RESET        0xA4

typedef struct {
    uint8_t  link_active;       /* 1 = GS link established  */
    uint8_t  rx_data_available; /* 1 = uplink data waiting  */
    uint8_t  tx_queue_full;
    int8_t   rssi_dbm;          /* Received signal strength */
} COMMS_Status_t_Info;

/* =========================================================
 * PUBLIC API
 * ========================================================= */

COMMS_Status_t COMMS_Init(SPI_HandleTypeDef *hspi);

COMMS_Status_t COMMS_GetStatus(SPI_HandleTypeDef  *hspi,
                                COMMS_Status_t_Info *status);

/**
 * @brief  Send a data payload to the comms controller for downlink.
 * @param  data    Pointer to payload buffer
 * @param  len     Length in bytes (<= COMMS_MAX_DOWNLINK_PAYLOAD)
 */
COMMS_Status_t COMMS_SendDownlink(SPI_HandleTypeDef *hspi,
                                   const uint8_t     *data,
                                   uint16_t           len);

/**
 * @brief  Poll for received telecommand data from the ground station.
 * @param  buf     Output buffer
 * @param  buf_len Size of output buffer
 * @param  rx_len  Actual bytes received (0 if none)
 */
COMMS_Status_t COMMS_ReceiveUplink(SPI_HandleTypeDef *hspi,
                                    uint8_t           *buf,
                                    uint16_t           buf_len,
                                    uint16_t          *rx_len);

#endif /* COMMS_DRIVER_H */
