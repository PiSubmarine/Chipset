/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32u0xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define ADC_BALLAST_Pin GPIO_PIN_0
#define ADC_BALLAST_GPIO_Port GPIOA
#define ADC_REG5_Pin GPIO_PIN_1
#define ADC_REG5_GPIO_Port GPIOA
#define ADC_REGPI_Pin GPIO_PIN_2
#define ADC_REGPI_GPIO_Port GPIOA
#define LED_REGPI_Pin GPIO_PIN_3
#define LED_REGPI_GPIO_Port GPIOA
#define LED_ACTIVITY_Pin GPIO_PIN_4
#define LED_ACTIVITY_GPIO_Port GPIOA
#define LED_REG5_Pin GPIO_PIN_5
#define LED_REG5_GPIO_Port GPIOA
#define BATCHG_SDA_Pin GPIO_PIN_6
#define BATCHG_SDA_GPIO_Port GPIOA
#define BATCHG_SCL_Pin GPIO_PIN_7
#define BATCHG_SCL_GPIO_Port GPIOA
#define BATCHG_INT_Pin GPIO_PIN_0
#define BATCHG_INT_GPIO_Port GPIOB
#define BATCHG_INT_EXTI_IRQn EXTI0_1_IRQn
#define BATMON_ALERT_Pin GPIO_PIN_1
#define BATMON_ALERT_GPIO_Port GPIOB
#define BATMON_ALERT_EXTI_IRQn EXTI0_1_IRQn
#define LED_REG12_Pin GPIO_PIN_2
#define LED_REG12_GPIO_Port GPIOB
#define CHIPSET_SCL_Pin GPIO_PIN_10
#define CHIPSET_SCL_GPIO_Port GPIOB
#define CHIPSET_SDA_Pin GPIO_PIN_11
#define CHIPSET_SDA_GPIO_Port GPIOB
#define REG5_EN_Pin GPIO_PIN_12
#define REG5_EN_GPIO_Port GPIOB
#define REG12_EN_Pin GPIO_PIN_13
#define REG12_EN_GPIO_Port GPIOB
#define LED_BAT_Pin GPIO_PIN_14
#define LED_BAT_GPIO_Port GPIOB
#define BUTTON_PWR_Pin GPIO_PIN_15
#define BUTTON_PWR_GPIO_Port GPIOB
#define REG12_PG_Pin GPIO_PIN_8
#define REG12_PG_GPIO_Port GPIOA
#define CHIPSET_INT_Pin GPIO_PIN_5
#define CHIPSET_INT_GPIO_Port GPIOB
#define RPI_SCL_Pin GPIO_PIN_6
#define RPI_SCL_GPIO_Port GPIOB
#define RPI_SDA_Pin GPIO_PIN_7
#define RPI_SDA_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
