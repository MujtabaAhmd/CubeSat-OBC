/**
 * @file    obc_config.h
 * @brief   Central configuration for CubeSat OBC firmware
 *
 * Hardware target : STM32F4 Discovery (STM32F407VGTx)
 * RTOS            : FreeRTOS
 *
 * Pin assignments and bus mappings are placeholders.
 * Verify against your actual hardware connections before flashing.
 */

#ifndef OBC_CONFIG_H
#define OBC_CONFIG_H

#include "stm32f4xx_hal.h"

/* =========================================================
 * SYSTEM CLOCKS
 * ========================================================= */
#define OBC_SYSCLK_MHZ          168U   /* STM32F407 max */

/* =========================================================
 * BUS HANDLES
 * Declare the actual handles in main.c / MX-generated code,
 * then extern them via this header.
 * ========================================================= */

/* I2C1 -- MPU6050 (gyro/accel) + HMC5883L (magnetometer) */
extern I2C_HandleTypeDef  hi2c1;
#define ADCS_I2C_HANDLE       hi2c1

/* SPI1 -- Comms system (AX.25 controller) */
extern SPI_HandleTypeDef  hspi1;
#define COMMS_SPI_HANDLE      hspi1

/* SPI2 -- Payload trigger/status */
extern SPI_HandleTypeDef  hspi2;
#define PAYLOAD_SPI_HANDLE    hspi2

/* SPI3 -- SD card */
extern SPI_HandleTypeDef  hspi3;
#define SDCARD_SPI_HANDLE     hspi3

/* UART2 -- EPS */
extern UART_HandleTypeDef huart2;
#define EPS_UART_HANDLE       huart2

/* ADC1 -- Sun sensors (solar panels) */
extern ADC_HandleTypeDef  hadc1;
#define SUNSENSOR_ADC_HANDLE  hadc1

/* =========================================================
 * DEVICE ADDRESSES (I2C, 7-bit)
 * ========================================================= */
#define MPU6050_I2C_ADDR      (0x68 << 1)   /* AD0 pin = GND */
#define HMC5883L_I2C_ADDR     (0x1E << 1)

/* =========================================================
 * CHIP SELECT PINS
 * Adjust port/pin to match your wiring.
 * ========================================================= */
#define COMMS_CS_PORT         GPIOA
#define COMMS_CS_PIN          GPIO_PIN_4

#define PAYLOAD_CS_PORT       GPIOB
#define PAYLOAD_CS_PIN        GPIO_PIN_12

#define SDCARD_CS_PORT        GPIOB
#define SDCARD_CS_PIN         GPIO_PIN_5

/* =========================================================
 * SUN SENSOR ADC CHANNELS
 * One channel per panel face. Extend as needed.
 * ========================================================= */
#define SUNSENSOR_CH_PLUS_X   ADC_CHANNEL_0
#define SUNSENSOR_CH_MINUS_X  ADC_CHANNEL_1
#define SUNSENSOR_CH_PLUS_Y   ADC_CHANNEL_2
#define SUNSENSOR_CH_MINUS_Y  ADC_CHANNEL_3
#define SUNSENSOR_CH_PLUS_Z   ADC_CHANNEL_4
#define SUNSENSOR_CH_MINUS_Z  ADC_CHANNEL_5
#define SUNSENSOR_NUM_FACES   6U

/* =========================================================
 * FREERTOS TASK PRIORITIES
 * Higher number = higher priority in FreeRTOS.
 * ========================================================= */
#define PRIORITY_SCHEDULER    ( tskIDLE_PRIORITY + 6 )
#define PRIORITY_FDIR         ( tskIDLE_PRIORITY + 5 )
#define PRIORITY_HOUSEKEEPING ( tskIDLE_PRIORITY + 4 )
#define PRIORITY_ADCS         ( tskIDLE_PRIORITY + 3 )
#define PRIORITY_COMMS        ( tskIDLE_PRIORITY + 3 )
#define PRIORITY_TELECOMMAND  ( tskIDLE_PRIORITY + 3 )
#define PRIORITY_PAYLOAD      ( tskIDLE_PRIORITY + 2 )
#define PRIORITY_DATAMGMT     ( tskIDLE_PRIORITY + 2 )

/* =========================================================
 * FREERTOS TASK STACK SIZES (in words, not bytes)
 * ========================================================= */
#define STACK_SCHEDULER       256U
#define STACK_FDIR            512U
#define STACK_HOUSEKEEPING    512U
#define STACK_ADCS            1024U
#define STACK_COMMS           512U
#define STACK_TELECOMMAND     512U
#define STACK_PAYLOAD         256U
#define STACK_DATAMGMT        512U

/* =========================================================
 * TIMING INTERVALS (ms)
 * ========================================================= */
#define HOUSEKEEPING_INTERVAL_MS   1000U
#define ADCS_CONTROL_INTERVAL_MS   100U    /* 10 Hz control loop */
#define COMMS_POLL_INTERVAL_MS     50U
#define EPS_POLL_INTERVAL_MS       500U
#define FDIR_CHECK_INTERVAL_MS     500U

/* =========================================================
 * EPS PACKET DEFINITION
 * Adjust field sizes to match your actual EPS protocol.
 * ========================================================= */
#define EPS_PACKET_SIZE        16U

typedef struct {
    float    battery_voltage_V;      /* Volts                   */
    float    battery_current_mA;     /* Milliamps               */
    uint8_t  power_rail_states;      /* Bitmask, one bit/rail   */
    uint8_t  activated_systems;      /* Bitmask                 */
    uint8_t  eps_status_flags;       /* EPS internal fault bits */
    uint8_t  checksum;
} EPS_Packet_t;

/* =========================================================
 * FAULT / FDIR THRESHOLDS
 * Tune these to your EPS and mission requirements.
 * ========================================================= */
#define FAULT_BATT_UNDERVOLTAGE_V   6.0f   /* Volts  -- placeholder */
#define FAULT_BATT_OVERCURRENT_MA   2000.0f
#define FAULT_COMMS_TIMEOUT_MS      30000U
#define FAULT_ADCS_TIMEOUT_MS       5000U

#endif /* OBC_CONFIG_H */
