/**
 * @file    fdir_app.c
 * @brief   FDIR application implementation
 */

#include "fdir_app.h"
#include "sat_modes.h"
#include "obc_config.h"
#include "FreeRTOS.h"
#include "task.h"
#include "obc_config.h"

/* =========================================================
 * GLOBAL FAULT REGISTER
 * ========================================================= */
volatile uint8_t g_fault_register = 0;

/* =========================================================
 * RECOVERY ACTION HANDLERS
 * One function per fault type.
 * These are stubs -- fill in real recovery logic for FM.
 * ========================================================= */

static void _recover_undervoltage(void)
{
    /*
     * Recovery: shed non-essential loads via EPS command.
     * Example: turn off payload and comms to preserve battery.
     *
     * EPS_SendCommand(&EPS_UART_HANDLE, EPS_CMD_SHED_PAYLOAD, 0);
     * EPS_SendCommand(&EPS_UART_HANDLE, EPS_CMD_SHED_COMMS, 0);
     */
}

static void _recover_overcurrent(void)
{
    /*
     * Recovery: command EPS to reset affected rail.
     *
     * EPS_SendCommand(&EPS_UART_HANDLE, EPS_CMD_RESET_RAIL, rail_id);
     */
}

static void _recover_eps_comms(void)
{
    /*
     * Recovery: attempt to reinitialise UART peripheral.
     * If EPS is unresponsive after N retries, declare EPS dead
     * and continue on battery reserves only.
     *
     * HAL_UART_DeInit(&EPS_UART_HANDLE);
     * HAL_UART_Init(&EPS_UART_HANDLE);
     */
}

static void _recover_comms_timeout(void)
{
    /*
     * Recovery: reset comms SPI and send reset command.
     *
     * COMMS_Init(&COMMS_SPI_HANDLE);
     */
}

static void _recover_adcs_sensor(void)
{
    /*
     * Recovery: reinitialise I2C bus and re-init sensors.
     *
     * HAL_I2C_DeInit(&ADCS_I2C_HANDLE);
     * HAL_I2C_Init(&ADCS_I2C_HANDLE);
     * MPU6050_Init(&ADCS_I2C_HANDLE, ...);
     * HMC5883L_Init(&ADCS_I2C_HANDLE, ...);
     */
}

/* =========================================================
 * PUBLIC API
 * ========================================================= */
void FDIR_RaiseFault(uint8_t fault_flag)
{
    taskENTER_CRITICAL();
    g_fault_register |= fault_flag;
    taskEXIT_CRITICAL();

    /* Immediately trigger failsafe mode */
    ModeSM_SetTrigger(TRIGGER_FAULT_DETECTED);
}

void FDIR_ClearFault(uint8_t fault_flag)
{
    taskENTER_CRITICAL();
    g_fault_register &= ~fault_flag;
    taskEXIT_CRITICAL();

    /* If no faults remain, signal recovery */
    if (g_fault_register == 0) {
        ModeSM_ClearTrigger(TRIGGER_FAULT_DETECTED);
        ModeSM_SetTrigger(TRIGGER_FAULT_CLEARED);
    }
}

uint8_t FDIR_AnyFaultActive(void)
{
    return (g_fault_register != 0) ? 1U : 0U;
}

/* =========================================================
 * FDIR FREERTOS TASK
 * ========================================================= */
void FDIR_Task(void *pvParameters)
{
    (void)pvParameters;

    for (;;) {
        uint8_t faults;

        taskENTER_CRITICAL();
        faults = g_fault_register;
        taskEXIT_CRITICAL();

        /* Attempt recovery for each active fault */
        if (faults & FAULT_FLAG_UNDERVOLTAGE)  _recover_undervoltage();
        if (faults & FAULT_FLAG_OVERCURRENT)   _recover_overcurrent();
        if (faults & FAULT_FLAG_EPS_COMMS)     _recover_eps_comms();
        if (faults & FAULT_FLAG_COMMS_TIMEOUT) _recover_comms_timeout();
        if (faults & FAULT_FLAG_ADCS_SENSOR)   _recover_adcs_sensor();

        /*
         * After recovery attempts, re-read fault register.
         * If all clear, FDIR_ClearFault() will signal mode SM.
         *
         * For now, manual clear is required (conservative approach).
         * Add automatic re-verification logic when recovery routines
         * are fully implemented.
         */

        vTaskDelay(pdMS_TO_TICKS(FDIR_CHECK_INTERVAL_MS));
    }
}
