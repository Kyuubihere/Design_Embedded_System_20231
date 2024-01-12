/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
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

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>

#include "i2c.h"
#include "gpio.h"
#include "tim.h"
#include "spi.h"
#include "usart.h"
//#include "iwdg.h"
#include "max30102.h"
#include "ili9341.h"
#include "touch.h"
#include "fonts.h"
#include "UartRingbuffer.h"
#include "common.h"
//#include "dht11.h"
//#include "delay_timer.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct
{
	float Temperature;
	float Humidity;
	uint8_t bpm;
	uint8_t spo2;
}Node;

typedef struct
{
	bool SIM;
	bool GPRS;
	bool MQTT;
	bool STT;
}SIM_t;

typedef struct
{
	uint32_t LCD_Time[3];
	uint32_t IRQ_Time[3];
	uint32_t Measure_Time[3];
}Time_check;

typedef enum
{
	SLEEP = 0,
	ACTIVE = 1,
}Device_t;
typedef struct
{
	char action[50];
	char elem[10];
	uint8_t idx;
}action_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
const char *TAG = "Task";

#define Timeout 			60000
#define MeasurePeriod 		3000
#define	ActionPeriod 		15000

Node Node_1 = {0, 0, 0, 0};
Time_check Time_keeper = {{0}, {0}, {0} };
Device_t Mode = ACTIVE;
myButton_t Button_1 = {30, 170, 20, 0, 0, ILI9341_LIGHTBLUE, false, NULL};
myButton_t Button_2 = {290, 170, 20, 0, 0, ILI9341_LIGHTBLUE, false, NULL};


/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */


/* USER CODE END Variables */
/* Definitions for LCD */
osThreadId_t LCDHandle;
const osThreadAttr_t LCD_attributes = {
  .name = "LCD",
  .stack_size = 200 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for IRQ */
osThreadId_t IRQHandle;
const osThreadAttr_t IRQ_attributes = {
  .name = "IRQ",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal,
};
/* Definitions for Uart_user */
osThreadId_t Uart_userHandle;
const osThreadAttr_t Uart_user_attributes = {
  .name = "Uart_user",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};
/* Definitions for SuperQueue */
osMessageQueueId_t SuperQueueHandle;
const osMessageQueueAttr_t SuperQueue_attributes = {
  .name = "SuperQueue"
};
/* Definitions for Timer01 */
osTimerId_t Timer01Handle;
const osTimerAttr_t Timer01_attributes = {
  .name = "Timer01"
};
/* Definitions for Timer02 */
osTimerId_t Timer02Handle;
const osTimerAttr_t Timer02_attributes = {
  .name = "Timer02"
};
/* Definitions for Timer03 */
osTimerId_t Timer03Handle;
const osTimerAttr_t Timer03_attributes = {
  .name = "Timer03"
};
/* Definitions for Touch_binary */
osSemaphoreId_t Touch_binaryHandle;
const osSemaphoreAttr_t Touch_binary_attributes = {
  .name = "Touch_binary"
};
/* Definitions for Uart_binary */
osSemaphoreId_t Uart_binaryHandle;
const osSemaphoreAttr_t Uart_binary_attributes = {
  .name = "Uart_binary"
};
/* Definitions for PcSema */
osSemaphoreId_t PcSemaHandle;
const osSemaphoreAttr_t PcSema_attributes = {
  .name = "PcSema"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void LCD_task(void *argument);
void IRQ_task(void *argument);
void Uart_task(void *argument);
void Action_Timer(void *argument);
void LCD_Timeout(void *argument);
void Measure_Timer(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */



  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* Create the semaphores(s) */
  /* creation of Touch_binary */
  Touch_binaryHandle = osSemaphoreNew(1, 1, &Touch_binary_attributes);

  /* creation of Uart_binary */
  Uart_binaryHandle = osSemaphoreNew(1, 1, &Uart_binary_attributes);

  /* creation of PcSema */
  PcSemaHandle = osSemaphoreNew(4, 4, &PcSema_attributes);

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* Create the timer(s) */
  /* creation of Timer01 */
  Timer01Handle = osTimerNew(Action_Timer, osTimerPeriodic, NULL, &Timer01_attributes);

  /* creation of Timer02 */
  Timer02Handle = osTimerNew(LCD_Timeout, osTimerOnce, NULL, &Timer02_attributes);

  /* creation of Timer03 */
  Timer03Handle = osTimerNew(Measure_Timer, osTimerPeriodic, NULL, &Timer03_attributes);

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* creation of SuperQueue */
  SuperQueueHandle = osMessageQueueNew (64, sizeof(uint16_t), &SuperQueue_attributes);

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of LCD */
  LCDHandle = osThreadNew(LCD_task, NULL, &LCD_attributes);

  /* creation of IRQ */
  IRQHandle = osThreadNew(IRQ_task, NULL, &IRQ_attributes);

  /* creation of Uart_user */
  Uart_userHandle = osThreadNew(Uart_task, NULL, &Uart_user_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_LCD_task */
/**
  * @brief  Function implementing the LCD thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_LCD_task */
void LCD_task(void *argument)
{
  /* USER CODE BEGIN LCD_task */
	char StrgTemp[6];
	char StrgHumd[3];
	char Strgbpm[4] ;
	char Strgspo2[4];
	//Display
	ILI9341_Unselect();
	ILI9341_TouchUnselect();
	ILI9341_Init();
	ILI9341_FillScreen(ILI9341_BLACK);
	//Background
	ILI9341_WriteString(10, 10, "Embedded Design System 20231", Font_11x18, ILI9341_WHITE, ILI9341_BLACK);
	ILI9341_WriteString(30, 40, "20202647", Font_7x10, ILI9341_WHITE, ILI9341_BLACK);
	ILI9341_WriteString(235, 40, "20202543", Font_7x10, ILI9341_WHITE, ILI9341_BLACK);
	ILI9341_DrawLine(0, 50, 320, 50, ILI9341_WHITE);
	ILI9341_DrawLine(0, 200, 320, 200, ILI9341_WHITE);
	ILI9341_WriteString(100, 210, "MANDEVICES", Font_11x18, ILI9341_RED, ILI9341_BLACK);

	//Data
	ILI9341_WriteString(10, 60, "Temperature:", Font_11x18, ILI9341_WHITE, ILI9341_BLACK);
	ftoa(Node_1.Temperature, StrgTemp, 2);
	ILI9341_WriteString(150, 60, StrgTemp, Font_11x18, ILI9341_ORANGE, ILI9341_BLACK);

	ILI9341_WriteString(10, 80, "Humidity:", Font_11x18, ILI9341_WHITE, ILI9341_BLACK);
	intToStr((int)Node_1.Humidity, StrgHumd, 2);
	ILI9341_WriteString(150, 80, StrgHumd, Font_11x18, ILI9341_BLUE, ILI9341_BLACK);
	ILI9341_WriteString(180, 80, "%", Font_11x18, ILI9341_BLUE, ILI9341_BLACK);

	ILI9341_WriteString(10, 100, "BPM:", Font_11x18, ILI9341_WHITE, ILI9341_BLACK);
	intToStr(Node_1.bpm, Strgbpm, 2);
	ILI9341_WriteString(150, 100, Strgbpm, Font_11x18, ILI9341_GREEN, ILI9341_BLACK);

	ILI9341_WriteString(10, 120, "Spo2:", Font_11x18, ILI9341_WHITE, ILI9341_BLACK);
	intToStr(Node_1.spo2, Strgspo2, 2);
	ILI9341_WriteString(150, 120, Strgspo2, Font_11x18, ILI9341_PINK, ILI9341_BLACK);

	//button
	ILI9341_DrawCircle(Button_1.pos_x, Button_1.pos_y, Button_1.shape_r, ILI9341_LIGHTBLUE);
	ILI9341_DrawCircle(Button_1.pos_x, Button_1.pos_y, Button_1.shape_r/4, ILI9341_LIGHTBLUE);
	ILI9341_DrawCircle(Button_2.pos_x, Button_2.pos_y, Button_2.shape_r, ILI9341_LIGHTBLUE);
	ILI9341_DrawCircle(Button_2.pos_x, Button_2.pos_y, Button_2.shape_r/4, ILI9341_LIGHTBLUE);

	//start timer and softimer
	osTimerStart(Timer01Handle, ActionPeriod);
	osTimerStart(Timer02Handle, Timeout);
	osTimerStart(Timer03Handle, MeasurePeriod);
	//ILI9341_WriteString(150, 180, "Start Timer", Font_11x18, ILI9341_PINK, ILI9341_BLACK);

	HAL_TIM_Base_Start_IT(&htim11);
  /* Infinite loop */
  for(;;)
  {
	osThreadSuspend(LCDHandle);
	Time_keeper.LCD_Time[0] = HAL_GetTick();
	//Update

	ftoa(Node_1.Temperature, StrgTemp, 2);
	ILI9341_WriteString(150, 60, StrgTemp, Font_11x18, ILI9341_ORANGE, ILI9341_BLACK);

	intToStr((int)Node_1.Humidity, StrgHumd, 2);
	ILI9341_WriteString(150, 80, StrgHumd, Font_11x18, ILI9341_BLUE, ILI9341_BLACK);
	ILI9341_WriteString(180, 80, "%", Font_11x18, ILI9341_BLUE, ILI9341_BLACK);

	intToStr(Node_1.bpm, Strgbpm, 2);
	ILI9341_WriteString(150, 100, Strgbpm, Font_11x18, ILI9341_GREEN, ILI9341_BLACK);

	intToStr(Node_1.spo2, Strgspo2, 2);
	ILI9341_WriteString(150, 120, Strgspo2, Font_11x18, ILI9341_PINK, ILI9341_BLACK);
	//check button state
	if (Button_1.state == 1)
	{
		ILI9341_FillCircle(Button_1.pos_x, Button_1.pos_y + 0.5, Button_1.shape_r, ILI9341_BLUE);
		ILI9341_DrawCircle(Button_1.pos_x, Button_1.pos_y, Button_1.shape_r/4, ILI9341_GREEN);
	}
	else
	{
		ILI9341_WriteString(200, 100, "nOK", Font_11x18, ILI9341_GREEN, ILI9341_BLACK);
		ILI9341_DrawCircle(Button_1.pos_x, Button_1.pos_y, Button_1.shape_r, ILI9341_LIGHTBLUE);
		ILI9341_DrawCircle(Button_1.pos_x, Button_1.pos_y, Button_1.shape_r/4, ILI9341_LIGHTBLUE);
	}
	HAL_UART_Transmit(&huart2, (uint8_t *) "Le\r\n", 3, 50);

	Time_keeper.LCD_Time[1] = HAL_GetTick();
	Time_keeper.LCD_Time[2] = Time_keeper.LCD_Time[1] - Time_keeper.LCD_Time[0];
  }
  /* USER CODE END LCD_task */
}

/* USER CODE BEGIN Header_IRQ_task */
/**
* @brief Function implementing the IRQ thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_IRQ_task */
void IRQ_task(void *argument)
{
  /* USER CODE BEGIN IRQ_task */
	uint32_t currentTick = 0, lastTick = 0;
	uint16_t x, y;
  /* Infinite loop */
  for(;;)
  {
	if (osSemaphoreAcquire(Touch_binaryHandle, portMAX_DELAY) == osOK)
	{
		Time_keeper.IRQ_Time[0] = HAL_GetTick();
		osDelay(100);
		//debounce irq touch
		currentTick = HAL_GetTick();
		osThreadResume(LCDHandle);
		if ((HAL_GPIO_ReadPin(T_IRQ_GPIO_Port, T_IRQ_Pin) == 0) && (currentTick - lastTick > 500))
		{
			lastTick = currentTick;

			//init spi touch
//			HAL_SPI_DeInit(&hspi1);
//			hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_128;
//			HAL_SPI_Init(&hspi1);
//			while(ILI9341_TouchGetCoordinates(&x, &y) != true);
//			ILI9341_WriteString(x, y, "touch", Font_11x18, ILI9341_GREEN, ILI9341_BLACK);
//			HAL_SPI_DeInit(&hspi2);
//			hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
//			HAL_SPI_Init(&hspi2);
			//reset Timeout Timer
			osTimerStop(Timer02Handle);
			osTimerStart(Timer02Handle, 100);

			//Handle touch
			if (Mode == SLEEP)
			{
				Mode = ACTIVE;
				HAL_GPIO_WritePin(LCD_LED_GPIO_Port, LCD_LED_Pin, 1);
//				ILI9341_WriteString(140, 170, "Wake", Font_11x18, ILI9341_GREEN, ILI9341_BLACK);
				osThreadResume(LCDHandle);
			}
			else
			{
//				//Touch Handler
//				if ((x >= Button_1.pos_y) && (x <= (Button_1.pos_y + Button_1.shape_r))
//						&& (y >= Button_1.pos_x) && (y <= (Button_1.pos_x + Button_1.shape_r)))
//				{
//					if (Button_1.state == false)
//					{
//						Button_1.state = true;
//						ILI9341_WriteString(180, 190, "okelah vcl", Font_11x18, ILI9341_YELLOW, ILI9341_BLACK);
//					}
//					else
//					{
//						Button_1.state = false;
//						ILI9341_WriteString(180, 190, "deooke", Font_11x18, ILI9341_YELLOW, ILI9341_BLACK);
//					}
//				}
			}
			osTimerStart(Timer02Handle, Timeout);
			Time_keeper.IRQ_Time[1] = HAL_GetTick();
			Time_keeper.IRQ_Time[2] = Time_keeper.IRQ_Time[1] - Time_keeper.IRQ_Time[0];
		}

	}

  }
  /* USER CODE END IRQ_task */
}

/* USER CODE BEGIN Header_Uart_task */
/**
* @brief Function implementing the Uart_user thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_Uart_task */
void Uart_task(void *argument)
{
  /* USER CODE BEGIN Uart_task */
	#define BUFFER_ACTION 50
	Ringbuf_init();
	char* action[BUFFER_ACTION];
	osThreadSuspend(Uart_userHandle);
	osSemaphoreAcquire(Uart_binaryHandle, portMAX_DELAY);
	ILI9341_WriteString(180, 190, "Rx mode", Font_11x18, ILI9341_YELLOW, ILI9341_BLACK);
	osSemaphoreRelease(Uart_binaryHandle);
	//osSemaphoreRelease(Uart_binaryHandle);
  /* Infinite loop */
  for(;;)
  {
	  if(Wait_for("action"))
	  {
		  Get_after("action", 10, action);
		  ILI9341_WriteString(180, 190, "Receive", Font_11x18, ILI9341_YELLOW, ILI9341_BLACK);
		  Uart_sendstring(action);
		  osThreadResume(LCDHandle);
		  osThreadResume(IRQHandle);
	  }
	  else
	  {
		  ILI9341_WriteString(180, 190, "Not_Receive", Font_11x18, ILI9341_YELLOW, ILI9341_BLACK);
		  osThreadResume(LCDHandle);
		  osThreadResume(IRQHandle);
	  }
  }
  /* USER CODE END Uart_task */
}

/* Action_Timer function */
void Action_Timer(void *argument)
{
  /* USER CODE BEGIN Action_Timer */

  /* USER CODE END Action_Timer */
}

/* LCD_Timeout function */
void LCD_Timeout(void *argument)
{
  /* USER CODE BEGIN LCD_Timeout */
	HAL_GPIO_WritePin(LCD_LED_GPIO_Port, LCD_LED_Pin, 0);
	Mode = SLEEP;
	osThreadSuspend(LCDHandle);
  /* USER CODE END LCD_Timeout */
}

/* Measure_Timer function */
void Measure_Timer(void *argument)
{
  /* USER CODE BEGIN Measure_Timer */
	ILI9341_WriteString(180, 190, "measure", Font_11x18, ILI9341_YELLOW, ILI9341_BLACK);
	Time_keeper.Measure_Time[0] = HAL_GetTick();

	Time_keeper.Measure_Time[1] = HAL_GetTick();
	Time_keeper.Measure_Time[2] = Time_keeper.Measure_Time[1] - Time_keeper.Measure_Time[0];

	osSemaphoreRelease(PcSemaHandle);

	if (Mode == ACTIVE)
		osThreadResume(LCDHandle);
  /* USER CODE END Measure_Timer */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == T_IRQ_Pin)
  {
	  	Button_1.state = !Button_1.state;
		osSemaphoreRelease(Touch_binaryHandle);
  }
}
void HAL_UART_RxHalfCpltCallback(UART_HandleTypeDef *huart)
{
	if (huart == &huart2)
	{
		ILI9341_WriteString(180, 190, "Rxt mode", Font_11x18, ILI9341_YELLOW, ILI9341_BLACK);
		osSemaphoreRelease(Uart_binaryHandle);
	}
}

/* USER CODE END Application */

