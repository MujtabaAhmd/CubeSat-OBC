/**
 * @file    fdir_app.h / fdir_app.c
 * @brief   Fault Detection, Isolation, and Recovery (FDIR)
 *
 * Active in: FAILSAFE mode
 * Fault flags can be raised from any task at any time.
 *
 * Fault table (placeholder -- expand with your team):
 *
 *  FAULT_FLAG_UNDERVOLTAGE   Battery below safe threshold
 *  FAULT_FLAG_OVERCURRENT    Overcurrent on EPS rail
 *  FAULT_FLAG_EPS_COMMS      EPS UART not responding
 *  FAULT_FLAG_COMMS_TIMEOUT  Comms controller not responding
 *  FAULT_FLAG_ADCS_SENSOR    IMU or magnetometer read failure
 *  FAULT_FLAG_EPS_INTERNAL   EPS reported internal error
 *  FAULT_FLAG_SD_ERROR       SD card write failure
 *
 * Recovery actions are intentionally conservative for EM phase.
 */

#ifndef FDIR_APP_H
#define FDIR_APP_H

#include "FreeRTOS.h"
#include "task.h"
#include <stdint.h>

/* Fault flag bitmask definitions */
#define FAULT_FLAG_UNDERVOLTAGE   (1 << 0)
#define FAULT_FLAG_OVERCURRENT    (1 << 1)
#define FAULT_FLAG_EPS_COMMS      (1 << 2)
#define FAULT_FLAG_COMMS_TIMEOUT  (1 << 3)
#define FAULT_FLAG_ADCS_SENSOR    (1 << 4)
#define FAULT_FLAG_EPS_INTERNAL   (1 << 5)
#define FAULT_FLAG_SD_ERROR       (1 << 6)

/* Global fault register -- readable by all tasks */
extern volatile uint8_t g_fault_register;

/**
 * @brief  Raise a fault flag (thread-safe).
 *         Automatically triggers transition to FAILSAFE.
 */
void FDIR_RaiseFault(uint8_t fault_flag);

/**
 * @brief  Clear a fault flag (thread-safe).
 *         Call after recovery action succeeds.
 */
void FDIR_ClearFault(uint8_t fault_flag);

/**
 * @brief  Returns non-zero if any fault is active.
 */
uint8_t FDIR_AnyFaultActive(void);

void FDIR_Task(void *pvParameters);

#endif /* FDIR_APP_H */
