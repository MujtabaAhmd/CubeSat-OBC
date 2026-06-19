/**
 * @file    comms_driver.c
 * @brief   Comms SPI driver implementation
 */

#include "comms_driver.h"
#include <string.h>

#define SPI_TIMEOUT_MS  50U

/* Internal CS assert/deassert */
static inline void _cs_low(void)
{
    HAL_GPIO_WritePin(COMMS_CS_PORT, COMMS_CS_PIN, GPIO_PIN_RESET);
}
static inline void _cs_high(void)
{
    HAL_GPIO_WritePin(COMMS_CS_PORT, COMMS_CS_PIN, GPIO_PIN_SET);
}

/* Send a single command byte and read one response byte */
static HAL_StatusTypeDef _spi_cmd(SPI_HandleTypeDef *hspi,
                                   uint8_t cmd, uint8_t *resp)
{
    uint8_t dummy = 0x00;
    HAL_StatusTypeDef s;

    _cs_low();
    s = HAL_SPI_TransmitReceive(hspi, &cmd, resp, 1, SPI_TIMEOUT_MS);
    _cs_high();
    (void)dummy;
    return s;
}

COMMS_Status_t COMMS_Init(SPI_HandleTypeDef *hspi)
{
    (void)hspi;
    _cs_high();  /* Deassert CS at startup */

    char message[] = "Test";

        /* * Use the downlink function to send multi-byte payloads.
         * We cast the char array to uint8_t* and pass the string length.
         */
    COMMS_SendDownlink(hspi, (uint8_t *)message, strlen(message));
    HAL_Delay(100);

    return COMMS_OK;
}

COMMS_Status_t COMMS_GetStatus(SPI_HandleTypeDef  *hspi,
                                COMMS_Status_t_Info *status)
{
    uint8_t cmd  = COMMS_CMD_GET_STATUS;
    uint8_t resp[4] = {0};

    _cs_low();
    HAL_SPI_Transmit(hspi, &cmd, 1, SPI_TIMEOUT_MS);
    HAL_SPI_Receive(hspi,  resp, 4, SPI_TIMEOUT_MS);
    _cs_high();

    /*
     * Parse response bytes.
     * Byte layout is a placeholder -- define with comms controller spec.
     */
    status->link_active        = (resp[0] & 0x01);
    status->rx_data_available  = (resp[0] & 0x02) >> 1;
    status->tx_queue_full      = (resp[0] & 0x04) >> 2;
    status->rssi_dbm           = (int8_t)resp[1];

    return COMMS_OK;
}

COMMS_Status_t COMMS_SendDownlink(SPI_HandleTypeDef *hspi,
                                   const uint8_t     *data,
                                   uint16_t           len)
{
    if (len > COMMS_MAX_DOWNLINK_PAYLOAD) return COMMS_ERR_BUFFER_FULL;

    uint8_t header[3];
    header[0] = COMMS_CMD_TX_DATA;
    header[1] = (uint8_t)((len >> 8) & 0xFF);
    header[2] = (uint8_t)(len & 0xFF);

    _cs_low();
    HAL_SPI_Transmit(hspi, header, 3, SPI_TIMEOUT_MS);
    HAL_SPI_Transmit(hspi, (uint8_t *)data, len, SPI_TIMEOUT_MS);
    _cs_high();

    return COMMS_OK;
}

COMMS_Status_t COMMS_ReceiveUplink(SPI_HandleTypeDef *hspi,
                                    uint8_t           *buf,
                                    uint16_t           buf_len,
                                    uint16_t          *rx_len)
{
    *rx_len = 0;

    /* Poll for available data */
    uint8_t cmd  = COMMS_CMD_RX_POLL;
    uint8_t resp[3] = {0};

    _cs_low();
    HAL_SPI_Transmit(hspi, &cmd, 1, SPI_TIMEOUT_MS);
    HAL_SPI_Receive(hspi, resp, 3, SPI_TIMEOUT_MS);
    _cs_high();

    uint16_t avail = ((uint16_t)resp[1] << 8) | resp[2];

    if (resp[0] != 0x01 || avail == 0) return COMMS_OK; /* No data */

    if (avail > buf_len) avail = buf_len;  /* Clamp to buffer */

    _cs_low();
    HAL_SPI_Receive(hspi, buf, avail, SPI_TIMEOUT_MS);
    _cs_high();

    *rx_len = avail;
    return COMMS_OK;
}
