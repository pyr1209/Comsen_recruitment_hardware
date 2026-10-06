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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>

#include "calc_app.h"        /* 界面、按键分派、渲染分派都在这一层 */
#include "calc_view.h"
#include "lcd1602.h"
#include "lcd_cgram.h"
#include "touch_filter.h"
#include "touch_model.h"
#include "ttp229.h"
#include "usb_device.h"
/*
 * 裸机主循环，不使用 FreeRTOS。本文件只做两件事：
 *   1. 芯片/外设初始化（CubeMX 生成的时钟、GPIO、TIM3、USB）；
 *   2. 主循环的节拍：按键 10 ms 采样、串口 20 ms 取一次、屏幕按需刷新（最快 50 ms）、
 *      心跳灯 500 ms 翻转，互不阻塞。
 * "哪个键归哪一页、哪一页画什么"全在 calc_app 里，这里一行界面逻辑都没有。
 */
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define KEY_POLL_PERIOD_MS    10U
#define SERIAL_POLL_PERIOD_MS 20U
#define LCD_REFRESH_PERIOD_MS 50U
#define LED_BLINK_PERIOD_MS   500U

#define LCD_COLUMNS           16U


/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef htim3;

/* USER CODE BEGIN PV */
static touch_model_t touch_model; /* 电极串扰的组合识别模型 */

/* 屏幕内容（由 app_render 生成） */
static char top_line[LCD_COLUMNS];
static char bottom_line[LCD_COLUMNS];
static uint8_t top_cursor;        /* 光标所在列，CALC_VIEW_NO_CURSOR 表示不显示 */
static uint8_t cursor_row;        /* 光标所在行：0 = 第 1 行，1 = 第 2 行 */

/* 影子缓冲：屏幕内容或光标变了才写 LCD */
static char top_shadow[LCD_COLUMNS];
static char bottom_shadow[LCD_COLUMNS];
static uint8_t cursor_shadow = 0U;
static uint8_t cursor_row_shadow = 0U;
static uint8_t shadow_valid;

static touch_filter_t key_filter;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM3_Init(void);

/* USER CODE BEGIN PFP */
static void key_poll(void);
static void render(void);
static void lcd_flush(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
  * @brief  按键采样与去抖，每 10 ms 调用一次。
  * @note   原始位图来自自己写的 ttp229.c，去抖来自自己写的 touch_filter.c。
  */
static void key_poll(void)
{
  static uint8_t key_sent;
  uint32_t resolved;
  uint8_t index;

  (void)touch_filter_update(&key_filter, ttp229_read_physical());

  /* 组合识别：电极之间会串扰，一个按键往往点亮好几个电极，
     这里按作者的默认标定模型还原成"用户真正想按的那个键"。 */
  resolved = touch_model_classify(&touch_model, key_filter.stable_bitmap);

  if (resolved == 0U)
  {
    key_sent = 0U;
    return;
  }

  /* 同一次按住只处理一次，松开后才允许下一次。 */
  if (key_sent != 0U)
  {
    return;
  }
  key_sent = 1U;

  /* 把单个按键位换算成键号。 */
  index = 0U;
  while ((resolved & (1UL << index)) == 0U)
  {
    index++;
  }

  /* 采样只负责"哪个键被按了"；SHIFT 上档与页面分派都在应用层。 */
  app_handle_key(index);
}

/**
  * @brief  让应用层生成两行内容与光标位置。
  * @note   哪个页面画哪两行、光标放哪一列，都由 calc_app 决定。
  */
static void render(void)
{
  app_render(top_line, bottom_line, &top_cursor, &cursor_row);
}

/**
  * @brief  内容或光标变了才写屏；LCD 写入很慢，必须靠影子缓冲去重。
  * @note   为了显示光标，这里统一用 write_frame（write_lines 会关掉光标）。
  */
static void lcd_flush(void)
{
  if ((shadow_valid != 0U) &&
      (memcmp(top_line, top_shadow, LCD_COLUMNS) == 0) &&
      (memcmp(bottom_line, bottom_shadow, LCD_COLUMNS) == 0) &&
      (top_cursor == cursor_shadow) &&
      (cursor_row == cursor_row_shadow))
  {
    return;
  }

  if (top_cursor < LCD_COLUMNS)
  {
    lcd1602_write_frame(top_line, bottom_line, 1U, cursor_row, top_cursor);
  }
  else
  {
    lcd1602_write_frame(top_line, bottom_line, 0U, 0U, 0U);
  }

  (void)memcpy(top_shadow, top_line, LCD_COLUMNS);
  (void)memcpy(bottom_shadow, bottom_line, LCD_COLUMNS);
  cursor_shadow = top_cursor;
  cursor_row_shadow = cursor_row;
  shadow_valid = 1U;
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  uint32_t next_key;
  uint32_t next_serial;
  uint32_t next_flush;
  uint32_t next_led;

  /* USER CODE BEGIN 1 */

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
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */

  if (HAL_TIM_Base_Start(&htim3) != HAL_OK)
  {
    Error_Handler();
  }

  /* 启动 USB 虚拟串口（CDC）。 */
  MX_USB_DEVICE_Init();

  lcd1602_init();
  lcd_cgram_define_angle();   /* 定义 ∠ 这个自定义字符（CGRAM 掉电即失） */
  lcd_cgram_define_game_sprites();  /* 定义小游戏用的精灵（槽 2-6） */
  touch_filter_init(&key_filter);
  touch_model_load_default(&touch_model);
  app_init();             /* 界面回算式页；各页面、历史、Ans、SHIFT 全部复位 */
  shadow_valid = 0U;      /* 影子缓冲还是空的，第一帧必定写屏 */

  next_key = HAL_GetTick();
  next_serial = HAL_GetTick();
  next_flush = HAL_GetTick();
  next_led = HAL_GetTick();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    const uint32_t now = HAL_GetTick();

    /* 1. 按键：每 10 ms 采样一次（去抖要求） */
    if ((int32_t)(now - next_key) >= 0)
    {
      next_key += KEY_POLL_PERIOD_MS;
      key_poll();
    }

    /* 2. 串口：每 20 ms 取一次环形缓冲 */
    if ((int32_t)(now - next_serial) >= 0)
    {
      next_serial += SERIAL_POLL_PERIOD_MS;
      app_poll_serial();
    }

    /* 3. 生成画面并按需刷屏，最快 50 ms 一次 */
    if ((int32_t)(now - next_flush) >= 0)
    {
      next_flush += LCD_REFRESH_PERIOD_MS;
      render();
      lcd_flush();
    }

    /* 4. 心跳灯：证明主循环没有卡住 */
    if ((int32_t)(now - next_led) >= 0)
    {
      next_led += LED_BLINK_PERIOD_MS;
      HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
    }

    /* 5. 小游戏：页面自己判断"在不在游戏界面、到没到 70 ms" */
    app_poll_game();

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
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
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USB;
  PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_PLL_DIV1_5;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 71;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 65535;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_12|GPIO_PIN_13
                          |GPIO_PIN_14|GPIO_PIN_15, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6|GPIO_PIN_8, GPIO_PIN_SET);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : PB10 PB11 PB12 PB13
                           PB14 PB15 PB6 PB8 */
  GPIO_InitStruct.Pin = GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_12|GPIO_PIN_13
                          |GPIO_PIN_14|GPIO_PIN_15|GPIO_PIN_6|GPIO_PIN_8;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : PB7 PB9 */
  GPIO_InitStruct.Pin = GPIO_PIN_7|GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM4 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM4) {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

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

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line number
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
