/**
 * @file    adcs_app.c
 * @brief   ADCS application implementation
 */

#include "adcs_app.h"
#include "obc_config.h"
#include "sat_modes.h"
#include "mpu6050.h"
#include "hmc5883l.h"
#include "sun_sensor.h"
#include "FreeRTOS.h"
#include "task.h"
#include <math.h>
#include <string.h>

/* =========================================================
 * SHARED STATE
 * ========================================================= */
ADCS_State_t g_adcs_state = {0};

/* Internal control mode */
static volatile ADCS_ControlMode_t g_control_mode = ADCS_MODE_DETUMBLE;

/* Previous magnetometer reading for B-dot */
static float g_mag_prev[3] = {0.0f, 0.0f, 0.0f};
static uint8_t g_first_run = 1U;

/* =========================================================
 * INTERNAL: BDOT DETUMBLE
 * Computes dipole moment command and sends to actuators.
 * ========================================================= */
static void _run_bdot(const HMC5883L_Data_t *mag,
                       float dt_s)
{
    float db[3];
    db[0] = (mag->mx - g_mag_prev[0]) / dt_s;
    db[1] = (mag->my - g_mag_prev[1]) / dt_s;
    db[2] = (mag->mz - g_mag_prev[2]) / dt_s;

    g_mag_prev[0] = mag->mx;
    g_mag_prev[1] = mag->my;
    g_mag_prev[2] = mag->mz;

    if (g_first_run) {
        g_first_run = 0;
        return;  /* Skip first iteration -- dB/dt is garbage */
    }

    /* Dipole command: m = -k * dB/dt */
    float m[3];
    m[0] = -ADCS_BDOT_GAIN * db[0];
    m[1] = -ADCS_BDOT_GAIN * db[1];
    m[2] = -ADCS_BDOT_GAIN * db[2];

    /*
     * TODO: Send m[] to magnetorquer driver.
     * Clamp to actuator limits before commanding.
     * Example:
     *   Magnetorquer_SetDipole(m[0], m[1], m[2]);
     */
    (void)m;
}

/* =========================================================
 * INTERNAL: NADIR POINTING (STUB)
 * Full implementation depends on actuator choice.
 * ========================================================= */
static void _run_nadir_pointing(const ADCS_State_t *state)
{
    /*
     * Nadir pointing controller stub.
     *
     * A full implementation would:
     * 1. Compute current attitude (quaternion) from sensor fusion
     *    (e.g. MEKF or TRIAD using mag + sun vector).
     * 2. Compute nadir direction in body frame from TLE/orbit data.
     * 3. Compute attitude error quaternion.
     * 4. Run PD controller -> torque command.
     * 5. Allocate torque to actuators.
     *
     * This is deferred to the CDR phase once the actuator suite
     * and orbit propagator (SGP4 or lookup table) are integrated.
     */
    (void)state;
}

/* =========================================================
 * INTERNAL: CHECK DETUMBLE COMPLETION
 * ========================================================= */
static uint8_t _is_detumbled(const MPU6050_Data_t *imu)
{
    float omega_mag = sqrtf(imu->gx * imu->gx +
                             imu->gy * imu->gy +
                             imu->gz * imu->gz);
    return (omega_mag < ADCS_DETUMBLE_THRESHOLD_DEG_S) ? 1U : 0U;
}

/* =========================================================
 * PUBLIC API
 * ========================================================= */
void ADCS_SetMode(ADCS_ControlMode_t mode)
{
    g_control_mode = mode;
    g_first_run    = 1U;   /* Reset derivative on mode switch */
}

/* =========================================================
 * ADCS FREERTOS TASK
 * ========================================================= */
void ADCS_Task(void *pvParameters)
{
    (void)pvParameters;

    MPU6050_Data_t  imu_data  = {0};
    HMC5883L_Data_t mag_data  = {0};
    SunSensor_Data_t sun_data = {0};

    float dt_s = (float)ADCS_CONTROL_INTERVAL_MS / 1000.0f;

    /* Determine initial mode from satellite mode */
    SatelliteMode_t sat_mode = ModeSM_GetMode();
    g_control_mode = (sat_mode == SAT_MODE_IMAGE_CAPTURE)
                     ? ADCS_MODE_NADIR
                     : ADCS_MODE_DETUMBLE;

    for (;;) {
        /* --------------------------------------------------
         * 1. Read all sensors
         * -------------------------------------------------- */
        MPU6050_ReadAll(&ADCS_I2C_HANDLE, &imu_data);
        HMC5883L_Read(&ADCS_I2C_HANDLE,  &mag_data);
        SunSensor_Read(&SUNSENSOR_ADC_HANDLE, &sun_data);

        /* --------------------------------------------------
         * 2. Update shared state (not mutex-protected here;
         *    housekeeping reads this with low criticality.
         *    Add mutex if strict data consistency is needed.)
         * -------------------------------------------------- */
        g_adcs_state.omega[0]     = imu_data.gx;
        g_adcs_state.omega[1]     = imu_data.gy;
        g_adcs_state.omega[2]     = imu_data.gz;
        g_adcs_state.mag_field[0] = mag_data.mx;
        g_adcs_state.mag_field[1] = mag_data.my;
        g_adcs_state.mag_field[2] = mag_data.mz;
        g_adcs_state.sun_vector[0] = sun_data.sun_vector[0];
        g_adcs_state.sun_vector[1] = sun_data.sun_vector[1];
        g_adcs_state.sun_vector[2] = sun_data.sun_vector[2];
        g_adcs_state.control_mode  = g_control_mode;

        /* --------------------------------------------------
         * 3. Run control law based on current mode
         * -------------------------------------------------- */
        switch (g_control_mode) {

            case ADCS_MODE_DETUMBLE:
                _run_bdot(&mag_data, dt_s);

                if (_is_detumbled(&imu_data)) {
                    g_adcs_state.detumble_done = 1.0f;
                    ModeSM_SetTrigger(TRIGGER_DETUMBLE_DONE);
                } else {
                    g_adcs_state.detumble_done = 0.0f;
                }
                break;

            case ADCS_MODE_NADIR:
                _run_nadir_pointing(&g_adcs_state);
                break;

            case ADCS_MODE_IDLE:
            default:
                /* No actuation -- sensors still read for housekeeping */
                break;
        }

        vTaskDelay(pdMS_TO_TICKS(ADCS_CONTROL_INTERVAL_MS));
    }
}
