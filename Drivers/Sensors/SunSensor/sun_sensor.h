/**
 * @file    sun_sensor.h / sun_sensor.c
 * @brief   Coarse sun sensor using solar panel open-circuit voltage (ADC)
 *
 * Each face's solar panel is read via an ADC channel. The
 * sun vector is estimated from the relative illumination of
 * each face. This is a coarse method -- accuracy is typically
 * within ~10-15 degrees depending on panel placement and
 * albedo environment. Sufficient for detumbling and rough
 * nadir pointing.
 *
 * IMPORTANT: Verify ADC channel-to-pin mapping in obc_config.h
 * against your actual wiring before use.
 */

#ifndef SUN_SENSOR_H
#define SUN_SENSOR_H

#include "stm32f4xx_hal.h"
#include "obc_config.h"
#include <stdint.h>

typedef struct {
    uint16_t raw[SUNSENSOR_NUM_FACES];  /* Raw ADC counts per face  */
    float    sun_vector[3];             /* Normalised body-frame [x,y,z] */
    uint8_t  sun_valid;                 /* 1 = eclipse not detected */
} SunSensor_Data_t;

/* Eclipse detection threshold -- tune to your panels */
#define SUNSENSOR_ECLIPSE_THRESHOLD   100U  /* ADC counts (0-4095) */

HAL_StatusTypeDef SunSensor_Read(ADC_HandleTypeDef *hadc,
                                  SunSensor_Data_t  *out);

#endif /* SUN_SENSOR_H */
