/**
 * @file    eps_driver.h / eps_driver.c
 * @brief   EPS communication driver over UART
 *
 * The EPS sends a packet at a fixed interval. The OBC polls
 * UART in a blocking fashion within the Housekeeping task.
 *
 * Protocol assumption: EPS transmits EPS_PACKET_SIZE bytes
 * at a time. No framing header is defined here -- add
 * start/stop bytes once your EPS firmware protocol is finalised.
 *
 * Baud rate: set in CubeMX (suggest 9600 or 115200 for EM).
 */

#ifndef EPS_DRIVER_H
#define EPS_DRIVER_H

#include "stm32f4xx_hal.h"
#include "obc_config.h"

/* Return codes */
typedef enum {
    EPS_OK             = 0,
    EPS_ERR_TIMEOUT    = 1,
    EPS_ERR_CHECKSUM   = 2,
    EPS_ERR_HAL        = 3,
} EPS_Status_t;

/**
 * @brief  Receive one EPS telemetry packet (blocking).
 * @param  huart  UART handle connected to EPS
 * @param  pkt    Output packet -- valid only if EPS_OK returned
 * @param  timeout_ms  Receive timeout in milliseconds
 * @return EPS_OK on success
 */
EPS_Status_t EPS_ReceivePacket(UART_HandleTypeDef *huart,
                                EPS_Packet_t       *pkt,
                                uint32_t            timeout_ms);

/**
 * @brief  Send a command byte to the EPS.
 *         Command set TBD -- placeholder for power rail control.
 */
EPS_Status_t EPS_SendCommand(UART_HandleTypeDef *huart,
                              uint8_t             cmd,
                              uint8_t             arg);

#endif /* EPS_DRIVER_H */
