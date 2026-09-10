/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "i2c.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
#define HMC5883L_ADDRESS    0x1E

#define HMC5883L_REG_CONFIGA    0x00
#define HMC5883L_REG_MODE       0x02
#define HMC5883L_REG_DATA       0x03

#define HMC5883L_MODEREG_BIT        1
#define HMC5883L_MODEREG_LENGTH     2

/* MPU6050 (GY-521) - address is 0x68 with AD0 pulled low, 0x69 if AD0 is pulled high */
#define MPU6050_ADDRESS         0x68
#define MPU6050_REG_PWR_MGMT_1  0x6B
#define MPU6050_REG_ACCEL_DATA  0x3B
#define MPU6050_REG_GYRO_DATA   0x43
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
int _write(int file, char *ptr, int len)
{
    for (int i = 0; i < len; i++)
    {
        ITM_SendChar((uint32_t)ptr[i]);
    }
    return len;
}
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void HMC5883L_Init(void)
{
    uint8_t data[2];
    data[0] = 0x02;
    data[1] = 0x00;
    HAL_I2C_Master_Transmit(&hi2c1, HMC5883L_ADDRESS << 1, data, 2, HAL_MAX_DELAY);
}

void HMC5883L_ReadData(int16_t* x, int16_t* y, int16_t* z)
{
    uint8_t buffer[6];
    HAL_I2C_Mem_Read(&hi2c1, HMC5883L_ADDRESS << 1, HMC5883L_REG_DATA, I2C_MEMADD_SIZE_8BIT, buffer, 6, HAL_MAX_DELAY);
    *x = (((int16_t)buffer[0]) << 8) | buffer[1];
    *y = (((int16_t)buffer[4]) << 8) | buffer[5];
    *z = (((int16_t)buffer[2]) << 8) | buffer[3];
}

/* Wakes the MPU6050 up by clearing the sleep bit in PWR_MGMT_1 (it powers up asleep) */
HAL_StatusTypeDef MPU6050_Init(void)
{
    uint8_t data[2];
    data[0] = MPU6050_REG_PWR_MGMT_1;
    data[1] = 0x00; /* clear SLEEP bit, use internal 8MHz oscillator */
    return HAL_I2C_Master_Transmit(&hi2c1, MPU6050_ADDRESS << 1, data, 2, HAL_MAX_DELAY);
}

/* Reads raw accelerometer counts. Divide by the sensitivity for your configured
   full-scale range (16384 LSB/g at default +-2g) to get g's - verify against
   your ACCEL_CONFIG register setting if you change the range. */
void MPU6050_ReadAccel(int16_t* ax, int16_t* ay, int16_t* az)
{
    uint8_t buffer[6];
    HAL_I2C_Mem_Read(&hi2c1, MPU6050_ADDRESS << 1, MPU6050_REG_ACCEL_DATA, I2C_MEMADD_SIZE_8BIT, buffer, 6, HAL_MAX_DELAY);
    *ax = (((int16_t)buffer[0]) << 8) | buffer[1];
    *ay = (((int16_t)buffer[2]) << 8) | buffer[3];
    *az = (((int16_t)buffer[4]) << 8) | buffer[5];
}

/* Reads raw gyroscope counts. Divide by the sensitivity for your configured
   full-scale range (131 LSB/(deg/s) at default +-250 deg/s) to get deg/s. */
void MPU6050_ReadGyro(int16_t* gx, int16_t* gy, int16_t* gz)
{
    uint8_t buffer[6];
    HAL_I2C_Mem_Read(&hi2c1, MPU6050_ADDRESS << 1, MPU6050_REG_GYRO_DATA, I2C_MEMADD_SIZE_8BIT, buffer, 6, HAL_MAX_DELAY);
    *gx = (((int16_t)buffer[0]) << 8) | buffer[1];
    *gy = (((int16_t)buffer[2]) << 8) | buffer[3];
    *gz = (((int16_t)buffer[4]) << 8) | buffer[5];
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
	uint8_t DataX[2];
	uint16_t Xaxis = 0;
	uint8_t DataY[2];
	uint16_t Yaxis = 0;
	uint8_t DataZ[2];
	uint16_t Zaxis = 0;

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */
  uint16_t address;
  char uartBuf[10];
  HAL_StatusTypeDef status;

  Xaxis = 0;
  Yaxis = 0;
  Zaxis = 0;

  HMC5883L_Init();

  status = MPU6050_Init();
  if (status != HAL_OK)
  {
      /* MPU6050 did not ACK - check wiring/address (0x68 vs 0x69) before continuing */
      Error_Handler();
  }

  int16_t x, y, z;
  int16_t ax, ay, az;
  int16_t gx, gy, gz;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	 HMC5883L_ReadData(&x, &y, &z);
	 MPU6050_ReadAccel(&ax, &ay, &az);
	 MPU6050_ReadGyro(&gx, &gy, &gz);

	 printf("MAG X: %d, Y: %d, Z: %d | ACCEL X: %d, Y: %d, Z: %d | GYRO X: %d, Y: %d, Z: %d\r\n",
	         x, y, z, ax, ay, az, gx, gy, gz);

	 HAL_Delay(1000);
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
