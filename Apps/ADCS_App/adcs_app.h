/**
 * @file    adcs_app.h / adcs_app.c
 * @brief   ADCS Application -- Detumble and Nadir Pointing
 *
 * Active in: ACTIVATION (detumble), IMAGE_CAPTURE (nadir pointing)
 *
 * Detumble algorithm: B-dot controller using magnetometer rate.
 *   Magnetic dipole command: m = -k * (dB/dt)
 *   This is a standard, well-validated algorithm for LEO cubesats.
 *
 * Nadir pointing: simplified gravity gradient / Sun-Nadir hybrid.
 *   Full implementation TBD once actuator suite is decided.
 *   Current stub reads sensors and publishes attitude estimate.
 *
 * Actuator interface: PLACEHOLDER.
 *   Define once magnetorquers / reaction wheels are selected.
 *
 * Control loop runs at ADCS_CONTROL_INTERVAL_MS (see obc_config.h).
 */

#ifndef ADCS_APP_H
#define ADCS_APP_H

#include "FreeRTOS.h"
#include "task.h"

typedef enum {
    ADCS_MODE_DETUMBLE = 0,
    ADCS_MODE_NADIR    = 1,
    ADCS_MODE_IDLE     = 2,
} ADCS_ControlMode_t;

typedef struct {
    float omega[3];        /* Angular velocity -- deg/s (body frame) */
    float mag_field[3];    /* Magnetic field -- Gauss (body frame)   */
    float sun_vector[3];   /* Sun vector -- normalised (body frame)  */
    float detumble_done;   /* 1.0 if angular rate below threshold    */
    ADCS_ControlMode_t control_mode;
} ADCS_State_t;

/* Shared state readable by other tasks (housekeeping, FDIR) */
extern ADCS_State_t g_adcs_state;

/* Angular rate threshold to declare detumble complete (deg/s) */
#define ADCS_DETUMBLE_THRESHOLD_DEG_S   2.0f

/* B-dot gain -- tune empirically. Units: A*m^2 / (T/s) */
#define ADCS_BDOT_GAIN                  1e4f

void ADCS_Task(void *pvParameters);

/**
 * @brief  Set the ADCS control mode.
 *         Called by the scheduler on mode transitions.
 */
void ADCS_SetMode(ADCS_ControlMode_t mode);

#endif /* ADCS_APP_H */
