/**
 * @file    housekeeping_app.c
 * @brief   Housekeeping application implementation
 */

#include "housekeeping_app.h"
#include "adcs_app.h"
#include "fdir_app.h"
#include "eps_driver.h"
#include "sat_modes.h"
#include "obc_config.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <string.h>

/* =========================================================
 * GLOBALS
 * ========================================================= */
QueueHandle_t g_hk_queue = NULL;

/* =========================================================
 * INTERNAL: COMPUTE SIMPLE XOR CHECKSUM
 * ========================================================= */
static uint8_t _frame_checksum(const HK_Frame_t *frame)
{
    const uint8_t *p   = (const uint8_t *)frame;
    uint16_t       len = sizeof(HK_Frame_t) - 1U;  /* Exclude checksum byte */
    uint8_t        cs  = 0;
    for (uint16_t i = 0; i < len; i++) cs ^= p[i];
    return cs;
}

/* =========================================================
 * INTERNAL: CHECK HEALTH AND RAISE FAULTS
 * ========================================================= */
static uint8_t _check_health(const EPS_Packet_t *eps)
{
    uint8_t faults = 0;

    if (eps->battery_voltage_V < FAULT_BATT_UNDERVOLTAGE_V) {
        faults |= FAULT_FLAG_UNDERVOLTAGE;
        FDIR_RaiseFault(FAULT_FLAG_UNDERVOLTAGE);
    }

    if (eps->battery_current_mA > FAULT_BATT_OVERCURRENT_MA) {
        faults |= FAULT_FLAG_OVERCURRENT;
        FDIR_RaiseFault(FAULT_FLAG_OVERCURRENT);
    }

    if (eps->eps_status_flags != 0) {
        faults |= FAULT_FLAG_EPS_INTERNAL;
        FDIR_RaiseFault(FAULT_FLAG_EPS_INTERNAL);
    }

    return faults;
}

/* =========================================================
 * HOUSEKEEPING FREERTOS TASK
 * ========================================================= */
void Housekeeping_Task(void *pvParameters)
{
    (void)pvParameters;

    /* Create HK queue once -- idempotent if called again */
    if (g_hk_queue == NULL) {
        g_hk_queue = xQueueCreate(HK_QUEUE_LENGTH, sizeof(HK_Frame_t));
        configASSERT(g_hk_queue != NULL);
    }

    EPS_Packet_t eps_pkt = {0};
    HK_Frame_t   frame   = {0};
    EPS_Status_t eps_status;

    for (;;) {
        /* --------------------------------------------------
         * 1. Poll EPS
         * -------------------------------------------------- */
        eps_status = EPS_ReceivePacket(&EPS_UART_HANDLE,
                                       &eps_pkt,
                                       EPS_POLL_INTERVAL_MS);

        /* --------------------------------------------------
         * 2. Assemble HK frame
         * -------------------------------------------------- */
        frame.timestamp_ms       = xTaskGetTickCount();
        frame.sat_mode           = (uint8_t)ModeSM_GetMode();

        if (eps_status == EPS_OK) {
            frame.batt_voltage_V    = eps_pkt.battery_voltage_V;
            frame.batt_current_mA   = eps_pkt.battery_current_mA;
            frame.eps_rail_states   = eps_pkt.power_rail_states;
            frame.eps_activated_sys = eps_pkt.activated_systems;
            frame.eps_flags         = eps_pkt.eps_status_flags;
        } else {
            /* EPS comms failure -- flag it */
            frame.eps_flags         = 0xFF;
            FDIR_RaiseFault(FAULT_FLAG_EPS_COMMS);
        }

        /* ADCS state snapshot (non-blocking read from shared struct) */
        frame.omega_x_deg_s  = g_adcs_state.omega[0];
        frame.omega_y_deg_s  = g_adcs_state.omega[1];
        frame.omega_z_deg_s  = g_adcs_state.omega[2];
        frame.mag_x_Gauss    = g_adcs_state.mag_field[0];
        frame.mag_y_Gauss    = g_adcs_state.mag_field[1];
        frame.mag_z_Gauss    = g_adcs_state.mag_field[2];
        frame.sun_valid      = (g_adcs_state.sun_vector[0] != 0.0f ||
                                 g_adcs_state.sun_vector[1] != 0.0f ||
                                 g_adcs_state.sun_vector[2] != 0.0f) ? 1 : 0;
        frame.sun_vec_x      = g_adcs_state.sun_vector[0];
        frame.sun_vec_y      = g_adcs_state.sun_vector[1];
        frame.sun_vec_z      = g_adcs_state.sun_vector[2];

        /* --------------------------------------------------
         * 3. Health check -- raises faults internally
         * -------------------------------------------------- */
        if (eps_status == EPS_OK) {
            frame.fault_flags = _check_health(&eps_pkt);
        }

        frame.checksum = _frame_checksum(&frame);

        /* --------------------------------------------------
         * 4. Push to HK queue for data management / downlink
         *    Use overwrite if full to always have latest data.
         * -------------------------------------------------- */
        if (xQueueSend(g_hk_queue, &frame, 0) != pdTRUE) {
            /* Queue full -- remove oldest and retry */
            HK_Frame_t discard;
            xQueueReceive(g_hk_queue, &discard, 0);
            xQueueSend(g_hk_queue, &frame, 0);
        }

        vTaskDelay(pdMS_TO_TICKS(HOUSEKEEPING_INTERVAL_MS));
    }
}
