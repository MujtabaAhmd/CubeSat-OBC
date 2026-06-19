/**
 * @file    sun_sensor.c
 * @brief   Sun sensor implementation (ADC polling)
 */

#include "sun_sensor.h"
#include <math.h>
#include <string.h>

/*
 * Face-to-body-axis mapping:
 *   raw[0] = +X face   raw[1] = -X face
 *   raw[2] = +Y face   raw[3] = -Y face
 *   raw[4] = +Z face   raw[5] = -Z face
 *
 * Adjust this to match your satellite body frame convention.
 */

static uint16_t _read_channel(ADC_HandleTypeDef *hadc, uint32_t channel)
{
    ADC_ChannelConfTypeDef cfg = {0};
    cfg.Channel      = channel;
    cfg.Rank         = 1;
    cfg.SamplingTime = ADC_SAMPLETIME_480CYCLES;

    HAL_ADC_ConfigChannel(hadc, &cfg);
    HAL_ADC_Start(hadc);
    HAL_ADC_PollForConversion(hadc, 10);
    uint16_t val = (uint16_t)HAL_ADC_GetValue(hadc);
    HAL_ADC_Stop(hadc);
    return val;
}

HAL_StatusTypeDef SunSensor_Read(ADC_HandleTypeDef *hadc,
                                   SunSensor_Data_t  *out)
{
    static const uint32_t channels[SUNSENSOR_NUM_FACES] = {
        SUNSENSOR_CH_PLUS_X,
        SUNSENSOR_CH_MINUS_X,
        SUNSENSOR_CH_PLUS_Y,
        SUNSENSOR_CH_MINUS_Y,
        SUNSENSOR_CH_PLUS_Z,
        SUNSENSOR_CH_MINUS_Z,
    };

    for (uint8_t i = 0; i < SUNSENSOR_NUM_FACES; i++) {
        out->raw[i] = _read_channel(hadc, channels[i]);
    }

    /* Coarse sun vector: differential illumination per axis */
    float sx = (float)out->raw[0] - (float)out->raw[1];
    float sy = (float)out->raw[2] - (float)out->raw[3];
    float sz = (float)out->raw[4] - (float)out->raw[5];

    float mag = sqrtf(sx*sx + sy*sy + sz*sz);

    /* Check for eclipse -- all panels dark */
    uint16_t max_reading = 0;
    for (uint8_t i = 0; i < SUNSENSOR_NUM_FACES; i++) {
        if (out->raw[i] > max_reading) max_reading = out->raw[i];
    }

    if (max_reading < SUNSENSOR_ECLIPSE_THRESHOLD || mag < 1.0f) {
        out->sun_valid = 0;
        out->sun_vector[0] = 0.0f;
        out->sun_vector[1] = 0.0f;
        out->sun_vector[2] = 0.0f;
    } else {
        out->sun_valid     = 1;
        out->sun_vector[0] = sx / mag;
        out->sun_vector[1] = sy / mag;
        out->sun_vector[2] = sz / mag;
    }

    return HAL_OK;
}
