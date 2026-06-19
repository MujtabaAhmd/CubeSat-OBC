/**
 * @file    hmc5883l.c
 * @brief   HMC5883L magnetometer driver implementation
 */

#include "hmc5883l.h"

#define I2C_TIMEOUT 10U

static float g_scale      = 1.0f / 1090.0f;  /* Default: +/-1.3 Ga gain */
static float g_offset[3]  = {0.0f, 0.0f, 0.0f};

static HAL_StatusTypeDef _write(I2C_HandleTypeDef *hi2c, uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    return HAL_I2C_Master_Transmit(hi2c, HMC5883L_ADDR, buf, 2, I2C_TIMEOUT);
}

HAL_StatusTypeDef HMC5883L_Init(I2C_HandleTypeDef *hi2c, HMC_Gain_t gain)
{
    HAL_StatusTypeDef s;

    /* Config A: 8 samples averaged, 15 Hz output rate, normal measurement */
    s = _write(hi2c, HMC_REG_CONFIG_A, 0x70);
    if (s != HAL_OK) return s;

    /* Config B: set gain */
    s = _write(hi2c, HMC_REG_CONFIG_B, (uint8_t)gain);
    if (s != HAL_OK) return s;

    /* Mode: continuous measurement */
    s = _write(hi2c, HMC_REG_MODE, 0x00);
    if (s != HAL_OK) return s;

    /* Store scale factor */
    switch (gain) {
        case HMC_GAIN_0_88: g_scale = 1.0f / 1370.0f; break;
        case HMC_GAIN_1_3:  g_scale = 1.0f / 1090.0f; break;
        case HMC_GAIN_1_9:  g_scale = 1.0f / 820.0f;  break;
        case HMC_GAIN_2_5:  g_scale = 1.0f / 660.0f;  break;
        case HMC_GAIN_4_0:  g_scale = 1.0f / 440.0f;  break;
        case HMC_GAIN_4_7:  g_scale = 1.0f / 390.0f;  break;
        case HMC_GAIN_5_6:  g_scale = 1.0f / 330.0f;  break;
        case HMC_GAIN_8_1:  g_scale = 1.0f / 230.0f;  break;
    }

    return HAL_OK;
}

HAL_StatusTypeDef HMC5883L_Read(I2C_HandleTypeDef *hi2c, HMC5883L_Data_t *out)
{
    uint8_t reg  = HMC_REG_DATAX_H;
    uint8_t raw[6];
    HAL_StatusTypeDef s;

    s = HAL_I2C_Master_Transmit(hi2c, HMC5883L_ADDR, &reg, 1, I2C_TIMEOUT);
    if (s != HAL_OK) return s;
    s = HAL_I2C_Master_Receive(hi2c, HMC5883L_ADDR, raw, 6, I2C_TIMEOUT);
    if (s != HAL_OK) return s;

    /* Byte order: XH XL ZH ZL YH YL  (note Z before Y -- datasheet) */
    int16_t rx = (int16_t)((uint16_t)raw[0] << 8 | raw[1]);
    int16_t rz = (int16_t)((uint16_t)raw[2] << 8 | raw[3]);
    int16_t ry = (int16_t)((uint16_t)raw[4] << 8 | raw[5]);

    /* 0x0F000 indicates ADC overflow -- flag as NaN or handle upstream */
    out->mx = ((float)rx * g_scale) - g_offset[0];
    out->my = ((float)ry * g_scale) - g_offset[1];
    out->mz = ((float)rz * g_scale) - g_offset[2];

    return HAL_OK;
}

void HMC5883L_SetHardIronOffset(float ox, float oy, float oz)
{
    g_offset[0] = ox;
    g_offset[1] = oy;
    g_offset[2] = oz;
}
