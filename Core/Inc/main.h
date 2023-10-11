/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2023 STMicroelectronics.
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
#include "stm32f1xx_hal.h"

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
#define Arriba_Pin GPIO_PIN_0
#define Arriba_GPIO_Port GPIOA
#define Abajo_Pin GPIO_PIN_1
#define Abajo_GPIO_Port GPIOA
#define Derecha_Pin GPIO_PIN_2
#define Derecha_GPIO_Port GPIOA
#define Izquierda_Pin GPIO_PIN_3
#define Izquierda_GPIO_Port GPIOA
#define Ok_Pin GPIO_PIN_4
#define Ok_GPIO_Port GPIOA
#define WP_Pin GPIO_PIN_5
#define WP_GPIO_Port GPIOA
#define A0_Pin GPIO_PIN_6
#define A0_GPIO_Port GPIOA
#define A1_Pin GPIO_PIN_7
#define A1_GPIO_Port GPIOA
#define Menu_IT_Pin GPIO_PIN_0
#define Menu_IT_GPIO_Port GPIOB
#define Menu_IT_EXTI_IRQn EXTI0_IRQn
#define Reset_IT_Pin GPIO_PIN_1
#define Reset_IT_GPIO_Port GPIOB
#define Reset_IT_EXTI_IRQn EXTI1_IRQn
#define A2_Pin GPIO_PIN_8
#define A2_GPIO_Port GPIOA
#define A3_Pin GPIO_PIN_9
#define A3_GPIO_Port GPIOA
#define A4_Pin GPIO_PIN_10
#define A4_GPIO_Port GPIOA
#define A5_Pin GPIO_PIN_11
#define A5_GPIO_Port GPIOA
#define A6_Pin GPIO_PIN_12
#define A6_GPIO_Port GPIOA
#define A7_Pin GPIO_PIN_3
#define A7_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
