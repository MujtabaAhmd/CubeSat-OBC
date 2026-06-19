/**
 * @file    housekeeping_app.h / housekeeping_app.c
 * @brief   Housekeeping Application
 *
 * Active in: ALL modes
 *
 * Responsibilities:
 *   - Poll EPS for power telemetry
 *   - Collect ADCS state snapshot
 *   - Pack a housekeeping telemetry frame
 *   - Store frame to SD card
 *   - Check system health and raise faults to FDIR
 *
 * Housekeeping data is also queued for downlink when the
 * satellite enters DOWNLINK mode.
 */

#ifndef HOUSEKEEPING_APP_H
#define HOUSEKEEPING_APP_H

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "obc_config.h"
#include <stdint.h>

/* Telemetry frame -- packed to minimise downlink bandwidth */
#pragma pack(push, 1)
typedef struct {
    uint32_t timestamp_ms;          /* OBC uptime in ms              */
    uint8_t  sat_mode;              /* Current operating mode        */
    float    batt_voltage_V;
    float    batt_current_mA;
    uint8_t  eps_rail_states;
    uint8_t  eps_activated_sys;
    uint8_t  eps_flags;
    float    omega_x_deg_s;         /* Angular velocity body frame   */
    float    omega_y_deg_s;
    float    omega_z_deg_s;
    float    mag_x_Gauss;
    float    mag_y_Gauss;
    float    mag_z_Gauss;
    uint8_t  sun_valid;
    float    sun_vec_x;
    float    sun_vec_y;
    float    sun_vec_z;
    uint8_t  fault_flags;           /* Bitmask -- see FDIR app       */
    uint8_t  checksum;
} HK_Frame_t;
#pragma pack(pop)

/* Queue for other tasks to consume HK frames (e.g. data management) */
extern QueueHandle_t g_hk_queue;

#define HK_QUEUE_LENGTH   8U

void Housekeeping_Task(void *pvParameters);

#endif /* HOUSEKEEPING_APP_H */
