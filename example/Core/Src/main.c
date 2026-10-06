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

#include "calc_format.h"
#include "calc_history.h"
#include "calc_input.h"
#include "calc_keymap.h"
#include "calc_page_game.h"
#include "calc_page_expr.h"
#include "calc_page_history.h"
#include "calc_page_menu.h"
#include "calc_page_serial.h"
#include "calc_result.h"
#include "calc_settings.h"
#include "calc_ui.h"
#include "calc_view.h"
#include "calculator_engine.h"
#include "lcd1602.h"
#include "lcd_cgram.h"
#include "touch_filter.h"
#include "touch_model.h"
#include "ttp229.h"
#include "usb_device.h"
#include "usbd_cdc_if.h"
/*
 * 裸机主循环，不使用 FreeRTOS：
 *   第 1 行显示算式（带硬件光标）或按键调试视图，第 2 行显示串口内容。
 *   按键每 10 ms 采样一次、串口每 20 ms 取一次、屏幕按需刷新，互不阻塞。
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
/* 算式本身（输入缓冲 + 显示窗口 + 键表）在 calc_page_expr 模块里。
   SHIFT 是输入层的事，留在这里：它只管"这一下要不要走上档层"。 */
static uint8_t shift_latched;     /* 上档锁存：结果行右端亮 'S'，下一个字符键走 SHIFT 层 */
static uint8_t shift_applied;     /* 本次按下的键是否走上档层 */
static touch_model_t touch_model; /* 电极串扰的组合识别模型 */

/* 菜单项。 */
/* 菜单页和三个设置页都在 calc_page_menu 模块里（高亮第几项、方向键怎么改
   暂存值都在那边），这里只负责把页面返回的动作落到实处。 */

/* 三个设置项（角度单位 / 数域 / 结果形式）都在 calc_settings 模块里，
   这里只通过它的接口读写（见 calc_settings.h）。 */

/* 屏幕内容（由 render 生成） */
static char top_line[LCD_COLUMNS];
static char result_line[LCD_COLUMNS];
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
static calc_history_t history;       /* 算式历史（最近 8 条） */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM3_Init(void);

/* USER CODE BEGIN PFP */
static void ui_handle_key(uint8_t index);
static void key_poll(void);
static void render(void);
static void lcd_flush(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
  * @brief  算式界面：把页面返回的"动作"落到实处。
  * @note   页面自己管输入缓冲、显示窗口和键表（calc_page_expr.c），
  *         牵动别的模块的事——开菜单、求值、清结果、重画结果——在这里做。
  */
static void handle_expr_key(uint8_t index)
{
  switch (expr_page_handle_key(index, shift_applied))
  {
    case EXPR_ACTION_OPEN_MENU:
      ui_switch_to(SCREEN_MENU);
      break;

    case EXPR_ACTION_EVALUATE:
      calc_result_evaluate(expr_page_input(), &history, 1U, result_line);
      break;

    case EXPR_ACTION_CLEAR_RESULT:      /* AC：输入已经清了，结果行回 READY */
      view_set_text(result_line, "READY");
      calc_result_clear_answer();
      break;

    case EXPR_ACTION_REFRESH_RESULT:    /* FMT：设置已切，按新形式重画上一次结果 */
      (void)calc_result_show_answer(result_line);
      break;

    default:
      break;
  }
}

/**
  * @brief  按当前界面把按键分派下去。
  */
static void ui_handle_key(uint8_t index)
{
  switch (ui_screen())
  {
    case SCREEN_MENU:
      switch (menu_page_handle_key(index))
      {
        /* 进设置页先把暂存值同步成当前生效值。 */
        case MENU_ACTION_OPEN_ANGLE:
          settings_begin(SETTING_ANGLE);
          ui_switch_to(SCREEN_ANGLE);
          break;
        case MENU_ACTION_OPEN_COMPLEX:
          settings_begin(SETTING_COMPLEX);
          ui_switch_to(SCREEN_COMPLEX);
          break;
        case MENU_ACTION_OPEN_POLAR:
          settings_begin(SETTING_POLAR);
          ui_switch_to(SCREEN_POLAR);
          break;
        case MENU_ACTION_OPEN_HISTORY:
          history_page_reset();          /* 进来先看最新一条，横向窗口归零 */
          ui_switch_to(SCREEN_HISTORY);
          break;
        case MENU_ACTION_OPEN_GAME:
          game_page_enter();             /* 开新的一局并切到游戏界面 */
          break;
        case MENU_ACTION_OPEN_SERIAL:
          ui_switch_to(SCREEN_SERIAL);
          break;
        case MENU_ACTION_EXIT_EXPR:
          ui_switch_to(SCREEN_EXPR);
          break;
        default:
          break;
      }
      break;

    case SCREEN_ANGLE:
    case SCREEN_COMPLEX:
    case SCREEN_POLAR:
    {
      /* 三个设置页共用一套按键处理；具体是哪一个由当前页面决定。 */
      const setting_id_t id = (ui_screen() == SCREEN_ANGLE) ? SETTING_ANGLE
                            : ((ui_screen() == SCREEN_COMPLEX) ? SETTING_COMPLEX
                                                               : SETTING_POLAR);

      switch (option_page_handle_key(index, id))
      {
        case OPTION_ACTION_APPLY_EXIT:
          /* 设置变了要重算一次：它不仅改以后的算法，也改已算出结果的样子。 */
          calc_result_reapply(expr_page_input(), result_line);
          ui_switch_to(SCREEN_EXPR);
          break;
        case OPTION_ACTION_EXIT_MENU:
          ui_switch_to(SCREEN_MENU);
          break;
        case OPTION_ACTION_EXIT_EXPR:
          ui_switch_to(SCREEN_EXPR);
          break;
        default:
          break;
      }
      break;
    }

    case SCREEN_SERIAL:
      serial_page_handle_key(index);
      break;

    case SCREEN_HISTORY:
    {
      /* 页面返回"要不要把某条装回输入行"；装入和重算是应用层的事，在这里做。 */
      const history_action_t action = history_page_handle_key(index, &history);

      if (action != HISTORY_ACTION_NONE)
      {
        if (history_page_load(&history, expr_page_input()) != 0U)
        {
          expr_page_window_reset();      /* 装入的算式从头看起 */

          if (action == HISTORY_ACTION_LOAD_EVAL)
          {
            calc_result_evaluate(expr_page_input(), &history, 1U, result_line);
          }
        }
      }
      break;
    }

    case SCREEN_GAME:
      game_page_handle_key(index);
      break;

    default:
      handle_expr_key(index);
      break;
  }
}

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

  /* SHIFT 目前只给按键调试视图用；真正的输入层语义下一步再做。 */
  if (index == TTP229_KEY_SHIFT)
  {
    shift_latched = (uint8_t)(shift_latched ^ 1U);
    shift_applied = 0U;
  }
  else
  {
    /* 别的键消费掉上档：这一下走上档层，之后锁存自动解除。 */
    shift_applied = shift_latched;
    if (shift_latched != 0U)
    {
      shift_latched = 0U;
    }
  }

  ui_handle_key(index);
}

/**
  * @brief  生成第 1 行的内容和光标位置。
  */
static void render(void)
{
  top_cursor = CALC_VIEW_NO_CURSOR;
  cursor_row = 0U;

  switch (ui_screen())
  {
    case SCREEN_MENU:
      menu_page_render(top_line, bottom_line);
      break;

    case SCREEN_ANGLE:
    case SCREEN_COMPLEX:
    case SCREEN_POLAR:
    {
      const setting_id_t id = (ui_screen() == SCREEN_ANGLE) ? SETTING_ANGLE
                            : ((ui_screen() == SCREEN_COMPLEX) ? SETTING_COMPLEX
                                                               : SETTING_POLAR);

      option_page_render(id, top_line, bottom_line);
      break;
    }

    case SCREEN_SERIAL:
      serial_page_render(top_line, bottom_line);
      break;

    case SCREEN_GAME:
      game_page_render(top_line, bottom_line);
      break;

    case SCREEN_HISTORY:
      history_page_render(&history, top_line, bottom_line);
      break;

    default:
      expr_page_render(top_line, &top_cursor);
      (void)memcpy(bottom_line, result_line, LCD_COLUMNS);

      /* 上档锁存时在结果行最右端亮一个 'S'：提示下一个字符键会走 SHIFT 层。
         结果文本最长也就十来个字符，右端这几格是空的，不会挡住结果。 */
      if (shift_latched != 0U)
      {
        bottom_line[LCD_COLUMNS - 1U] = 'S';
      }
      break;
  }
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
  expr_page_init();
  shift_latched = 0U;
  ui_init();              /* 上电进算式界面 */
  menu_page_init();
  settings_init();        /* 角度单位 DEG / 数域 CMPLX / 结果形式 RECT */
  shadow_valid = 0U;
  view_set_text(result_line, "READY");
  serial_page_init();
  calc_result_init();     /* 清空"上一次结果"（Ans） */
  calc_history_clear(&history);
  history_page_reset();
  game_page_init();

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
      serial_page_poll();
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
    game_page_poll();

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
