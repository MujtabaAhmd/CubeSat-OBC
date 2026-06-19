/**
 * @file    hmc5883l.h / hmc5883l.c
 * @brief   GY-273 / HMC5883L 3-axis magnetometer driver (I2C)
 *
 * Output: calibrated magnetic field in Gauss (X, Y, Z).
 * Hard-iron offsets are zero by default -- calibrate on the
 * fully assembled satellite to remove PCB interference.
 */

#ifndef HMC5883L_H
#define HMC5883L_H

#include "stm32f4xx_hal.h"

/* I2C address (fixed by device, 7-bit) */
#define HMC5883L_ADDR       (0x1E << 1)

/* Register addresses */
#define HMC_REG_CONFIG_A    0x00
#define HMC_REG_CONFIG_B    0x01
#define HMC_REG_MODE        0x02
#define HMC_REG_DATAX_H     0x03
#define HMC_REG_STATUS      0x09
#define HMC_REG_ID_A        0x0A

/* Gain options -- affects scale (Gauss/LSB) */
typedef enum {
    HMC_GAIN_0_88 = 0x00,   /* +/- 0.88 Ga, 1370 LSB/Ga */
    HMC_GAIN_1_3  = 0x20,   /* +/- 1.3  Ga,  1090 LSB/Ga  (default) */
    HMC_GAIN_1_9  = 0x40,
    HMC_GAIN_2_5  = 0x60,
    HMC_GAIN_4_0  = 0x80,
    HMC_GAIN_4_7  = 0xA0,
    HMC_GAIN_5_6  = 0xC0,
    HMC_GAIN_8_1  = 0xE0,
} HMC_Gain_t;

typedef struct {
    float mx, my, mz;    /* Gauss */
} HMC5883L_Data_t;

HAL_StatusTypeDef HMC5883L_Init(I2C_HandleTypeDef *hi2c, HMC_Gain_t gain);
HAL_StatusTypeDef HMC5883L_Read(I2C_HandleTypeDef *hi2c, HMC5883L_Data_t *out);

/**
 * @brief  Set hard-iron offsets (Gauss). Measure by rotating
 *         the satellite and using (max+min)/2 per axis.
 */
void HMC5883L_SetHardIronOffset(float ox, float oy, float oz);

#endif /* HMC5883L_H */
