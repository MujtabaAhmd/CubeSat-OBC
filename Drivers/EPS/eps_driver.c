/**
 * @file    eps_driver.c
 * @brief   EPS UART driver implementation
 */

#include "eps_driver.h"
#include <string.h>

/* Simple XOR checksum over first (EPS_PACKET_SIZE-1) bytes */
static uint8_t _compute_checksum(const uint8_t *data, uint16_t len)
{
    uint8_t cs = 0;
    for (uint16_t i = 0; i < len; i++) cs ^= data[i];
    return cs;
}

EPS_Status_t EPS_ReceivePacket(UART_HandleTypeDef *huart,
                                EPS_Packet_t       *pkt,
                                uint32_t            timeout_ms)
{
    uint8_t raw[EPS_PACKET_SIZE];
    HAL_StatusTypeDef s;

    s = HAL_UART_Receive(huart, raw, EPS_PACKET_SIZE, timeout_ms);
    if (s == HAL_TIMEOUT) return EPS_ERR_TIMEOUT;
    if (s != HAL_OK)      return EPS_ERR_HAL;

    /* Verify checksum */
    uint8_t expected = _compute_checksum(raw, EPS_PACKET_SIZE - 1U);
    if (expected != raw[EPS_PACKET_SIZE - 1U]) return EPS_ERR_CHECKSUM;

    /*
     * Deserialise raw bytes into struct.
     * This assumes little-endian floats and a specific byte layout.
     * MUST match whatever your EPS firmware packs.
     * Adjust offsets once the EPS protocol is locked.
     */
    memcpy(&pkt->battery_voltage_V,  &raw[0], 4);
    memcpy(&pkt->battery_current_mA, &raw[4], 4);
    pkt->power_rail_states  = raw[8];
    pkt->activated_systems  = raw[9];
    pkt->eps_status_flags   = raw[10];
    pkt->checksum           = raw[EPS_PACKET_SIZE - 1U];

    return EPS_OK;
}

EPS_Status_t EPS_SendCommand(UART_HandleTypeDef *huart,
                              uint8_t             cmd,
                              uint8_t             arg)
{
    uint8_t buf[3] = {cmd, arg, (uint8_t)(cmd ^ arg)};
    HAL_StatusTypeDef s = HAL_UART_Transmit(huart, buf, sizeof(buf), 50U);
    return (s == HAL_OK) ? EPS_OK : EPS_ERR_HAL;
}
