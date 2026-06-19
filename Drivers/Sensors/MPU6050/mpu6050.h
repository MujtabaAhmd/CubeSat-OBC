/**
 * @file    mpu6050.h
 * @brief   MPU6050 IMU driver (I2C, polling mode)
 *
 * Provides raw and calibrated readings for:
 *   - 3-axis accelerometer (g)
 *   - 3-axis gyroscope (deg/s)
 *   - Die temperature (deg C)
 *
 * Calibration offsets are zeroed by default. Run
 * MPU6050_Calibrate() once on a stable platform to populate them.
 */

#ifndef MPU6050_H
#define MPU6050_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* =========================================================
 * REGISTER MAP (partial -- enough for this firmware)
 * ========================================================= */
#define MPU6050_REG_PWR_MGMT_1    0x6B
#define MPU6050_REG_SMPLRT_DIV    0x19
#define MPU6050_REG_CONFIG        0x1A
#define MPU6050_REG_GYRO_CONFIG   0x1B
#define MPU6050_REG_ACCEL_CONFIG  0x1C
#define MPU6050_REG_ACCEL_XOUT_H  0x3B
#define MPU6050_REG_TEMP_OUT_H    0x41
#define MPU6050_REG_GYRO_XOUT_H   0x43
#define MPU6050_REG_WHO_AM_I      0x75

/* =========================================================
 * FULL-SCALE RANGE OPTIONS
 * ========================================================= */
typedef enum {
    MPU6050_ACCEL_FS_2G  = 0x00,
    MPU6050_ACCEL_FS_4G  = 0x08,
    MPU6050_ACCEL_FS_8G  = 0x10,
    MPU6050_ACCEL_FS_16G = 0x18,
} MPU6050_AccelFS_t;

typedef enum {
    MPU6050_GYRO_FS_250  = 0x00,
    MPU6050_GYRO_FS_500  = 0x08,
    MPU6050_GYRO_FS_1000 = 0x10,
    MPU6050_GYRO_FS_2000 = 0x18,
} MPU6050_GyroFS_t;

/* =========================================================
 * DATA STRUCTURES
 * ========================================================= */
typedef struct {
    float ax, ay, az;    /* Accelerometer -- g          */
    float gx, gy, gz;    /* Gyroscope     -- deg/s      */
    float temp_C;        /* Temperature   -- deg Celsius */
} MPU6050_Data_t;

/* =========================================================
 * PUBLIC API
 * ========================================================= */

HAL_StatusTypeDef MPU6050_Init(I2C_HandleTypeDef *hi2c,
                               MPU6050_AccelFS_t   accel_range,
                               MPU6050_GyroFS_t    gyro_range);

HAL_StatusTypeDef MPU6050_ReadAll(I2C_HandleTypeDef *hi2c,
                                   MPU6050_Data_t    *out);

/**
 * @brief  Collect ~200 samples and compute average offset.
 *         Call with sensor at rest, axes aligned with gravity.
 *         Stores result internally; subsequent ReadAll() calls
 *         subtract the offsets.
 */
HAL_StatusTypeDef MPU6050_Calibrate(I2C_HandleTypeDef *hi2c);

#endif /* MPU6050_H */
