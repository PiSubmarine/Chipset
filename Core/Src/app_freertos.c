/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : app_freertos.c
  * Description        : FreeRTOS applicative file
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
#include "app_freertos.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
typedef StaticTask_t osStaticThreadDef_t;
typedef StaticSemaphore_t osStaticMutexDef_t;
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for PowerTask */
osThreadId_t PowerTaskHandle;
uint32_t PowerTaskBuffer[ 128 ];
osStaticThreadDef_t PowerTaskControlBlock;
const osThreadAttr_t PowerTask_attributes = {
  .name = "PowerTask",
  .stack_mem = &PowerTaskBuffer[0],
  .stack_size = sizeof(PowerTaskBuffer),
  .cb_mem = &PowerTaskControlBlock,
  .cb_size = sizeof(PowerTaskControlBlock),
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for AdcTask */
osThreadId_t AdcTaskHandle;
uint32_t AdcTaskBuffer[ 86 ];
osStaticThreadDef_t AdcTaskControlBlock;
const osThreadAttr_t AdcTask_attributes = {
  .name = "AdcTask",
  .stack_mem = &AdcTaskBuffer[0],
  .stack_size = sizeof(AdcTaskBuffer),
  .cb_mem = &AdcTaskControlBlock,
  .cb_size = sizeof(AdcTaskControlBlock),
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for HostProtocolTask */
osThreadId_t HostProtocolTaskHandle;
uint32_t HostProtocolTaskBuffer[ 128 ];
osStaticThreadDef_t HostProtocolTaskControlBlocTask;
const osThreadAttr_t HostProtocolTask_attributes = {
  .name = "HostProtocolTask",
  .stack_mem = &HostProtocolTaskBuffer[0],
  .stack_size = sizeof(HostProtocolTaskBuffer),
  .cb_mem = &HostProtocolTaskControlBlocTask,
  .cb_size = sizeof(HostProtocolTaskControlBlocTask),
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for SharedStateMutex */
osMutexId_t SharedStateMutexHandle;
osStaticMutexDef_t SharedStateMutexControlBlock;
const osMutexAttr_t SharedStateMutex_attributes = {
  .name = "SharedStateMutex",
  .cb_mem = &SharedStateMutexControlBlock,
  .cb_size = sizeof(SharedStateMutexControlBlock),
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

/* USER CODE BEGIN 1 */
/* Functions needed when configGENERATE_RUN_TIME_STATS is on */
__weak void configureTimerForRunTimeStats(void)
{

}

__weak unsigned long getRunTimeCounterValue(void)
{
return 0;
}
/* USER CODE END 1 */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */
  /* creation of SharedStateMutex */
  SharedStateMutexHandle = osMutexNew(&SharedStateMutex_attributes);

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */
  /* creation of PowerTask */
  PowerTaskHandle = osThreadNew(StartPowerTask, NULL, &PowerTask_attributes);

  /* creation of AdcTask */
  AdcTaskHandle = osThreadNew(StartAdcTask, NULL, &AdcTask_attributes);

  /* creation of HostProtocolTask */
  HostProtocolTaskHandle = osThreadNew(StartHostProtocolTask, NULL, &HostProtocolTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

