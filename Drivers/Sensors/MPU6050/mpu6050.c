/**
 * @file    mpu6050.c
 * @brief   MPU6050 driver implementation
 */

#include "mpu6050.h"
#include <string.h>

/* =========================================================
 * MODULE-PRIVATE STATE
 * ========================================================= */
static float g_accel_scale = 1.0f;   /* LSB to g        */
static float g_gyro_scale  = 1.0f;   /* LSB to deg/s    */

/* Calibration offsets (raw LSB units) */
static float g_accel_offset[3] = {0.0f, 0.0f, 0.0f};
static float g_gyro_offset[3]  = {0.0f, 0.0f, 0.0f};

#define I2C_TIMEOUT_MS   10U
#define MPU6050_ADDR     (0x68 << 1)

/* =========================================================
 * INTERNAL HELPERS
 * ========================================================= */
static HAL_StatusTypeDef _write_reg(I2C_HandleTypeDef *hi2c,
                                     uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    return HAL_I2C_Master_Transmit(hi2c, MPU6050_ADDR,
                                   buf, 2, I2C_TIMEOUT_MS);
}

static HAL_StatusTypeDef _read_regs(I2C_HandleTypeDef *hi2c,
                                     uint8_t reg,
                                     uint8_t *dst, uint16_t len)
{
    HAL_StatusTypeDef s;
    s = HAL_I2C_Master_Transmit(hi2c, MPU6050_ADDR,
                                &reg, 1, I2C_TIMEOUT_MS);
    if (s != HAL_OK) return s;
    return HAL_I2C_Master_Receive(hi2c, MPU6050_ADDR,
                                  dst, len, I2C_TIMEOUT_MS);
}

static int16_t _to_int16(uint8_t high, uint8_t low)
{
    return (int16_t)((uint16_t)high << 8 | (uint16_t)low);
}

/* =========================================================
 * PUBLIC API
 * ========================================================= */
HAL_StatusTypeDef MPU6050_Init(I2C_HandleTypeDef *hi2c,
                                MPU6050_AccelFS_t   accel_range,
                                MPU6050_GyroFS_t    gyro_range)
{
    HAL_StatusTypeDef s;

    /* Wake device, use internal 8 MHz oscillator */
    s = _write_reg(hi2c, MPU6050_REG_PWR_MGMT_1, 0x00);
    if (s != HAL_OK) return s;

    /* Sample rate = 8000 / (1 + SMPLRT_DIV) -- set to ~1 kHz */
    s = _write_reg(hi2c, MPU6050_REG_SMPLRT_DIV, 0x07);
    if (s != HAL_OK) return s;

    /* DLPF config -- 94 Hz bandwidth */
    s = _write_reg(hi2c, MPU6050_REG_CONFIG, 0x02);
    if (s != HAL_OK) return s;

    /* Gyro full scale */
    s = _write_reg(hi2c, MPU6050_REG_GYRO_CONFIG, (uint8_t)gyro_range);
    if (s != HAL_OK) return s;

    /* Accel full scale */
    s = _write_reg(hi2c, MPU6050_REG_ACCEL_CONFIG, (uint8_t)accel_range);
    if (s != HAL_OK) return s;

    /* Compute scale factors */
    /* Accel: FS 2g -> 16384 LSB/g, 4g -> 8192, 8g -> 4096, 16g -> 2048 */
    switch (accel_range) {
        case MPU6050_ACCEL_FS_2G:  g_accel_scale = 1.0f / 16384.0f; break;
        case MPU6050_ACCEL_FS_4G:  g_accel_scale = 1.0f / 8192.0f;  break;
        case MPU6050_ACCEL_FS_8G:  g_accel_scale = 1.0f / 4096.0f;  break;
        case MPU6050_ACCEL_FS_16G: g_accel_scale = 1.0f / 2048.0f;  break;
    }

    /* Gyro: FS 250 -> 131 LSB/deg/s, 500 -> 65.5, 1000 -> 32.8, 2000 -> 16.4 */
    switch (gyro_range) {
        case MPU6050_GYRO_FS_250:  g_gyro_scale = 1.0f / 131.0f;  break;
        case MPU6050_GYRO_FS_500:  g_gyro_scale = 1.0f / 65.5f;   break;
        case MPU6050_GYRO_FS_1000: g_gyro_scale = 1.0f / 32.8f;   break;
        case MPU6050_GYRO_FS_2000: g_gyro_scale = 1.0f / 16.4f;   break;
    }

    return HAL_OK;
}

HAL_StatusTypeDef MPU6050_ReadAll(I2C_HandleTypeDef *hi2c,
                                   MPU6050_Data_t    *out)
{
    uint8_t raw[14];
    HAL_StatusTypeDef s = _read_regs(hi2c, MPU6050_REG_ACCEL_XOUT_H,
                                     raw, sizeof(raw));
    if (s != HAL_OK) return s;

    /* Bytes 0-5: accel X,Y,Z  6-7: temp  8-13: gyro X,Y,Z */
    int16_t ax_raw = _to_int16(raw[0],  raw[1]);
    int16_t ay_raw = _to_int16(raw[2],  raw[3]);
    int16_t az_raw = _to_int16(raw[4],  raw[5]);
    int16_t t_raw  = _to_int16(raw[6],  raw[7]);
    int16_t gx_raw = _to_int16(raw[8],  raw[9]);
    int16_t gy_raw = _to_int16(raw[10], raw[11]);
    int16_t gz_raw = _to_int16(raw[12], raw[13]);

    out->ax     = ((float)ax_raw - g_accel_offset[0]) * g_accel_scale;
    out->ay     = ((float)ay_raw - g_accel_offset[1]) * g_accel_scale;
    out->az     = ((float)az_raw - g_accel_offset[2]) * g_accel_scale;
    out->gx     = ((float)gx_raw - g_gyro_offset[0])  * g_gyro_scale;
    out->gy     = ((float)gy_raw - g_gyro_offset[1])  * g_gyro_scale;
    out->gz     = ((float)gz_raw - g_gyro_offset[2])  * g_gyro_scale;
    out->temp_C = (float)t_raw / 340.0f + 36.53f;

    return HAL_OK;
}

HAL_StatusTypeDef MPU6050_Calibrate(I2C_HandleTypeDef *hi2c)
{
    #define CALIB_SAMPLES 200U

    MPU6050_Data_t reading;
    double accel_sum[3] = {0}, gyro_sum[3] = {0};

    for (uint32_t i = 0; i < CALIB_SAMPLES; i++) {
        if (MPU6050_ReadAll(hi2c, &reading) != HAL_OK) return HAL_ERROR;
        accel_sum[0] += reading.ax; accel_sum[1] += reading.ay; accel_sum[2] += reading.az;
        gyro_sum[0]  += reading.gx; gyro_sum[1]  += reading.gy; gyro_sum[2]  += reading.gz;
        HAL_Delay(5);
    }

    /*
     * Store offsets in LSB space by working backward through scale.
     * For accel Z axis we subtract 1g (gravity) -- assumes Z-up orientation.
     * Adjust axis if your board orientation differs.
     */
    g_accel_offset[0] = (float)(accel_sum[0] / CALIB_SAMPLES) / g_accel_scale;
    g_accel_offset[1] = (float)(accel_sum[1] / CALIB_SAMPLES) / g_accel_scale;
    g_accel_offset[2] = (float)(accel_sum[2] / CALIB_SAMPLES) / g_accel_scale
                        - (1.0f / g_accel_scale);  /* remove 1g */

    g_gyro_offset[0]  = (float)(gyro_sum[0] / CALIB_SAMPLES) / g_gyro_scale;
    g_gyro_offset[1]  = (float)(gyro_sum[1] / CALIB_SAMPLES) / g_gyro_scale;
    g_gyro_offset[2]  = (float)(gyro_sum[2] / CALIB_SAMPLES) / g_gyro_scale;

    return HAL_OK;
}
