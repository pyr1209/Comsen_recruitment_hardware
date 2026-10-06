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
#include "calc_result.h"
#include "calc_settings.h"
#include "calc_ui.h"
#include "calc_view.h"
#include "calculator_engine.h"
#include "dino_game.h"
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
#define TTP229_KEY_COUNT      30U

/* 功能键的键号，其余键号是字符键。 */
#define TTP229_KEY_SHIFT      0U
#define TTP229_KEY_BACK       1U
#define TTP229_KEY_MODE       2U
#define TTP229_KEY_UP         3U
#define TTP229_KEY_OK         4U
#define TTP229_KEY_LEFT       7U
#define TTP229_KEY_DOWN       8U
#define TTP229_KEY_RIGHT      9U
#define TTP229_KEY_DEL        13U
#define TTP229_KEY_AC         14U
#define TTP229_KEY_FMT        28U
#define TTP229_KEY_EXE        29U

#define NO_CURSOR             0xFFU
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef htim3;

/* USER CODE BEGIN PV */
/* 每个键往算式里插入的字符，'\0' 表示这个键不插入字符。
   与作者 primary 表一致：键 27 插入 'E'（科学计数法的指数记号）。 */
static const char key_insert_char[TTP229_KEY_COUNT] =
{
  '\0',  /* 00 SHIFT */
  '\0',  /* 01 BACK */
  '\0',  /* 02 MODE */
  '\0',  /* 03 UP */
  '\0',  /* 04 OK */
  '(',   /* 05 */
  ')',   /* 06 */
  '\0',  /* 07 LEFT */
  '\0',  /* 08 DOWN */
  '\0',  /* 09 RIGHT */
  '7', '8', '9',
  '\0',  /* 13 DEL */
  '\0',  /* 14 AC */
  '4', '5', '6', '*', '/',
  '1', '2', '3', '+', '-',
  '0', '.', 'E',
  '\0',  /* 28 FMT */
  '\0'   /* 29 EXE */
};

/* SHIFT 层往算式里插入的记号，取自作者的 shifted 表：
   'p'=π、'd'=∠、'i'、'e'、'l'=log、'n'=ln、'^'=x^y、'q'=√、
   's'=sin、'c'=cos、't'=tan、'A'=Ans。'\0' 表示这个键没有上档。
   其中 c l n p q s t 是函数，插入时会自动补一个左括号。
   Ans 放在 28 号键的上档：键帽上 28 是大字 FMT、小字 ANS，小字就是上档层
   （作者的表把 'A' 放在 27，和键帽丝印不一致，这里照键帽走）。 */
static const char key_insert_shifted[TTP229_KEY_COUNT] =
{
  '\0', '\0', '\0', '\0', '\0',   /* 00-04 无上档 */
  '\0', '\0', '\0', '\0', '\0',   /* 05-09 无上档 */
  'p',  (char)LCD1602_CHAR_ANGLE, 'i',   /* 10-12：π ∠ i；∠ 用 CGRAM 字符码 */
  '\0', '\0',                     /* 13-14 */
  'e',  'l',  'n',  '^',  'q',    /* 15-19：e log ln x^y √ */
  's',  'c',  't',                /* 20-22：sin cos tan */
  '\0', '\0',                     /* 23-24 */
  '\0', '\0',                     /* 25-26 */
  '\0',                           /* 27：键帽上没有上档记号 */
  'A',                            /* 28：ANS —— 插入上一次的结果 */
  '\0'                            /* 29 */
};

/* 输入缓冲与显示窗口 */
static calc_input_t calc_input;
static uint8_t window_start;      /* 窗口左端在算式里的下标 */
static uint8_t shift_latched;     /* 上档锁存：结果行右端亮 'S'，下一个字符键走 SHIFT 层 */
static uint8_t shift_applied;     /* 本次按下的键是否走上档层 */
static touch_model_t touch_model; /* 电极串扰的组合识别模型 */

/* 菜单项。 */
#define MENU_ITEM_ANGLE    0U
#define MENU_ITEM_COMPLEX  1U
#define MENU_ITEM_POLAR    2U
#define MENU_ITEM_HISTORY  3U
#define MENU_ITEM_GAME     4U
#define MENU_ITEM_SERIAL   5U
#define MENU_ITEM_COUNT    6U

static const char *const menu_item_names[MENU_ITEM_COUNT] =
{
  "ANGLE UNIT",
  "COMPLEX",
  "POLAR",
  "HISTORY",
  "GAME",
  "SEND FROM PC"
};

static uint8_t menu_index;        /* 菜单里高亮的项 */

/* 三个设置项（角度单位 / 数域 / 结果形式）和极坐标原点都在 calc_settings 模块里，
   这里只通过它的接口读写（见 calc_settings.h）。 */

/* 屏幕内容（由 render 生成） */
static char top_line[LCD_COLUMNS];
static char result_line[LCD_COLUMNS];
static char serial_line[LCD_COLUMNS];
static char bottom_line[LCD_COLUMNS];
static uint8_t top_cursor;        /* 光标所在列，NO_CURSOR 表示不显示 */
static uint8_t cursor_row;        /* 光标所在行：0 = 第 1 行，1 = 第 2 行 */

/* 影子缓冲：屏幕内容或光标变了才写 LCD */
static char top_shadow[LCD_COLUMNS];
static char bottom_shadow[LCD_COLUMNS];
static uint8_t cursor_shadow = 0U;
static uint8_t cursor_row_shadow = 0U;
static uint8_t shadow_valid;

static touch_filter_t key_filter;
static calc_history_t history;       /* 算式历史（最近 8 条） */
static uint8_t history_index;        /* 正在看第几条：0 = 最新 */
static uint8_t history_window;       /* 长算式的横向显示窗口 */
static dino_game_t dino_game;        /* 小游戏状态 */
static uint32_t next_game_tick;      /* 下一个游戏逻辑步的时刻（ms） */
static uint8_t game_sky_night;       /* 当前 CGRAM 天空那格是白天还是黑夜（0xFF = 未知） */
static calc_complex_t pending_origin;
static calc_input_t origin_backup;   /* 进原点页面时把算式缓冲整体存这里 */
static uint8_t origin_backup_window;
static uint8_t origin_from_menu;     /* 1 = 从 POLAR 菜单页进来的 */
static calc_status_t origin_status;  /* 原点页面上次提交的结果：非 OK 时第 2 行显示错误 */
static uint8_t serial_length;
static uint8_t serial_started;
static uint8_t swallow_next_lf;
static const char startup_banner[] = "SEND FROM PC    ";
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM3_Init(void);

/* USER CODE BEGIN PFP */
static uint8_t input_ends_with_operand(void);
static void insert_function_template(const char *name);
static void ui_handle_key(uint8_t index);
static void handle_expr_key(uint8_t index);
static void handle_menu_key(uint8_t index);
static void handle_option_page(uint8_t index, setting_id_t id);
static void handle_view_key(uint8_t index);
static void handle_history_key(uint8_t index);
static void handle_game_key(uint8_t index);
static void handle_origin_key(uint8_t index);
static void origin_begin(uint8_t from_menu);
static void origin_finish(uint8_t commit);
static calc_status_t origin_parse(calc_complex_t *value);
static void format_origin_line(calc_complex_t origin, char line[LCD_COLUMNS]);
static void format_game_over_line(uint16_t score, char line[LCD_COLUMNS]);
static void history_load(uint8_t evaluate);
static void key_poll(void);
static void serial_handle_byte(uint8_t byte);
static void serial_poll(void);
static void render(void);
static void lcd_flush(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
  * @brief  把一个按键翻译成缓冲操作或界面动作。
  * @note   进来之前已经过 touch_model_classify 判定，一次只处理一个键。
  */
static void handle_expr_key(uint8_t index)
{
  switch (index)
  {
      case TTP229_KEY_BACK:
        /* 作者的设计：BACK 相当于计算器的 CE，清掉整行输入，不是删一个字。 */
        calc_input_clear(&calc_input);
        break;

      case TTP229_KEY_DEL:
        /* 作者的设计：DEL 是退格，删光标左边一个字符。 */
        (void)calc_input_backspace(&calc_input);
        break;

      case TTP229_KEY_AC:
        /* 作者的设计：AC 除了清空输入，还要把界面拉回初始状态（READY）。 */
        calc_input_clear(&calc_input);
        view_set_text(result_line, "READY");
        calc_result_clear_answer();
        break;

      case TTP229_KEY_LEFT:
        (void)calc_input_move_left(&calc_input);
        break;

      case TTP229_KEY_RIGHT:
        (void)calc_input_move_right(&calc_input);
        break;

      case TTP229_KEY_UP:      /* 单行算式：上 = 移到开头 */
        (void)calc_input_move_home(&calc_input);
        break;

      case TTP229_KEY_DOWN:    /* 下 = 移到末尾 */
        (void)calc_input_move_end(&calc_input);
        break;

      case TTP229_KEY_MODE:    /* 作者的设计：MODE 打开模式菜单 */
        ui_switch_to(SCREEN_MENU);
        break;

      case TTP229_KEY_OK:
        /* OK 按作者的语义留给模式菜单确认，这里不做任何事。 */
        break;

      case TTP229_KEY_EXE:
        calc_result_evaluate(&calc_input, &history, 1U, result_line);   /* EXE 的结果进历史 */
        break;

      default:
      {
        /* 上档层：SHIFT 之后按的字符键插入上档记号；上档层没有定义的键不插入。
           动作键（BACK/DEL/AC/方向键…）在上面各自的分支里已经处理完了，
           它们照常执行，只是顺便消费掉上档状态。 */
        const char upper = key_insert_shifted[index];
        const char base = key_insert_char[index];

        if ((shift_applied != 0U) && (upper != '\0'))
        {
          if (upper == 'l')
          {
            /* log 是二元函数：插入 "l(,)"，光标停在第一个参数（底数）位置。
               逗号只占一格，"底数末尾"和"真数开头"本来就是相邻的两格，
               按一下 → 自然就过去了，所以这里不需要额外的跳过标记。 */
            (void)calc_input_insert(&calc_input, 'l');
            (void)calc_input_insert(&calc_input, '(');
            (void)calc_input_insert(&calc_input, ',');
            (void)calc_input_insert(&calc_input, ')');
            (void)calc_input_move_left(&calc_input);
            (void)calc_input_move_left(&calc_input);
          }
          else if (upper == 'n')
          {
            /* 自然对数：插入 ln()，光标停在括号里 */
            insert_function_template("ln");
          }
          else if (upper == 'q')
          {
            /* 平方根：插入 sqrt()，光标停在括号里 */
            insert_function_template("sqrt");
          }
          else if (upper == 'p')
          {
            /* π 写成两个字母 pi，表达式里一眼能看懂 */
            (void)calc_input_insert(&calc_input, 'p');
            (void)calc_input_insert(&calc_input, 'i');
          }
          else if (upper == '^')
          {
            if (input_ends_with_operand() != 0U)
            {
              /* 底数已经输好了：只补 "^()"，光标落在指数位置 */
              (void)calc_input_insert(&calc_input, '^');
              (void)calc_input_insert(&calc_input, '(');
              (void)calc_input_insert(&calc_input, ')');
              (void)calc_input_move_left(&calc_input);
            }
            else
            {
              /* 还没输底数：插完整的 "()^()"，光标落在第一个括号里。
                 第一个括号的右括号、^ 和第二个左括号都标成跳过的，
                 这样在底数后面按一下 → 就直接到指数位置。 */
              (void)calc_input_insert(&calc_input, '(');
              (void)calc_input_insert(&calc_input, ')');
              (void)calc_input_insert(&calc_input, '^');
              (void)calc_input_insert(&calc_input, '(');
              (void)calc_input_insert(&calc_input, ')');
              calc_input_mark_skip(&calc_input, 1U, 3U);    /* )^( */
              (void)calc_input_move_left(&calc_input);      /* 一次就退到第一个括号里 */
            }
          }
          else
          {
            /* 三角函数：插入 s() / c() / t()，光标停在括号里 */
            if ((upper == 's') || (upper == 'c') || (upper == 't'))
            {
              const char name[2] = { upper, '\0' };

              insert_function_template(name);
            }
            else
            {
              (void)calc_input_insert(&calc_input, upper);
            }
          }
        }
        else if ((shift_applied == 0U) && (index == TTP229_KEY_FMT))
        {
          /* 无上档的 FMT：在 a+bi 和 r∠θ 之间一键切换。
             往极坐标切的时候先弹出"原点"输入页（极坐标相对哪个点算），
             填完 OK 才真正切过去；切回 a+bi 不需要原点，直接切。 */
          if (settings_value(SETTING_POLAR) == 0U)
          {
            origin_begin(0U);
          }
          else
          {
            settings_set_value(SETTING_POLAR, 0U);   /* 快捷切换：生效值和暂存值一起改 */
            (void)calc_result_show_answer(result_line);
          }
        }
        else if ((shift_applied == 0U) && (base != '\0'))
        {
          /* 右括号：右边已经有模板自带的 ')' 时只把光标移过去，不插重复的。 */
          if ((base == ')') && (calc_input.cursor < calc_input.length) &&
              (calc_input.text[calc_input.cursor] == ')'))
          {
            (void)calc_input_move_right(&calc_input);
          }
          else
          {
            (void)calc_input_insert(&calc_input, base);
          }
        }
        break;
      }
  }
}

/**
  * @brief  菜单界面：方向键换项，OK 进入，BACK / MODE 退出。
  * @note   方向映射照作者的设计：UP/LEFT 往上、DOWN/RIGHT 往下。
  */
static void handle_menu_key(uint8_t index)
{
  switch (index)
  {
    case TTP229_KEY_UP:
    case TTP229_KEY_LEFT:
      menu_index = (menu_index == 0U) ? (uint8_t)(MENU_ITEM_COUNT - 1U)
                                      : (uint8_t)(menu_index - 1U);
      break;

    case TTP229_KEY_DOWN:
    case TTP229_KEY_RIGHT:
      menu_index = (uint8_t)((menu_index + 1U) % MENU_ITEM_COUNT);
      break;

    case TTP229_KEY_OK:
      switch (menu_index)
      {
        /* 进设置页时把暂存值同步成当前生效值。 */
        case MENU_ITEM_ANGLE:
          settings_begin(SETTING_ANGLE);
          ui_switch_to(SCREEN_ANGLE);
          break;
        case MENU_ITEM_COMPLEX:
          settings_begin(SETTING_COMPLEX);
          ui_switch_to(SCREEN_COMPLEX);
          break;
        case MENU_ITEM_POLAR:
          settings_begin(SETTING_POLAR);
          ui_switch_to(SCREEN_POLAR);
          break;
        case MENU_ITEM_HISTORY:
          history_index = 0U;      /* 进来先看最新一条 */
          history_window = 0U;
          ui_switch_to(SCREEN_HISTORY);
          break;
        case MENU_ITEM_GAME:
          dino_game_reset(&dino_game);        /* 每次进来都是新的一局 */
          next_game_tick = HAL_GetTick();
          game_sky_night = 0xFFU;             /* 让第一个逻辑步把天色点阵刷成白天 */
          ui_switch_to(SCREEN_GAME);
          break;
        case MENU_ITEM_SERIAL:  ui_switch_to(SCREEN_SERIAL);  break;
        default:                break;
      }
      break;

    case TTP229_KEY_BACK:
    case TTP229_KEY_MODE:
      ui_switch_to(SCREEN_EXPR);
      break;

    default:
      break;
  }
}

/**
  * @brief  设置页：方向键只改暂存值；OK 提交并重新应用，BACK / MODE 丢弃。
  * @note   "重新应用"是有必要的：设置不改变以后的算法，还会改变已经算出来的
  *         结果该怎么显示（角度单位、复数显示格式）。所以提交时要重算一次。
  *         提交完直接回结果界面（而不是回菜单）：改设置的目的是马上看结果
  *         变成什么样，回菜单等于多按一次。想继续改别的设置再按 MODE 进菜单。
  */
/**
  * @brief  设置页的按键：把键号翻译成对 calc_settings 的操作 + 页面导航。
  * @note   设置的"值语义"（暂存/提交/丢弃）在 calc_settings 里；这里只管
  *         两件界面的事：哪个键对应什么动作、以及按完留在哪个页面。
  *         另外"提交后要不要重算"也由界面决定（重算属于结果页的事）。
  */
static void handle_option_page(uint8_t index, setting_id_t id)
{
  switch (index)
  {
    case TTP229_KEY_UP:
    case TTP229_KEY_LEFT:
      settings_select(id, 0U);             /* 只改暂存值 */
      break;

    case TTP229_KEY_DOWN:
    case TTP229_KEY_RIGHT:
      settings_select(id, 1U);
      break;

    case TTP229_KEY_OK:
      if (settings_commit(id) != 0U)       /* 只有真的改了才重算一次 */
      {
        calc_result_reapply(&calc_input, result_line);
      }
      ui_switch_to(SCREEN_EXPR);
      break;

    case TTP229_KEY_BACK:
      settings_discard(id);                /* 丢弃：暂存恢复成生效值 */
      ui_switch_to(SCREEN_MENU);
      break;

    case TTP229_KEY_MODE:
      settings_discard(id);                /* 直接退出也要丢弃 */
      ui_switch_to(SCREEN_EXPR);
      break;

    default:
      break;
  }
}

/**
  * @brief  查看页（串口 / 按键调试）：BACK 回菜单，MODE 直接回算式界面。
  */
static void handle_view_key(uint8_t index)
{
  switch (index)
  {
    case TTP229_KEY_BACK:
      ui_switch_to(SCREEN_MENU);
      break;

    case TTP229_KEY_MODE:
      ui_switch_to(SCREEN_EXPR);
      break;

    default:
      break;
  }
}

/**
  * @brief  把当前选中那条历史装进输入缓冲，回算式界面。
  * @note   evaluate = 1 时顺便立刻重算一次（相当于"再来一遍"）。
  */
static void history_load(uint8_t evaluate)
{
  const calc_history_entry_t *entry = calc_history_get(&history, history_index);
  uint8_t position = 0U;

  if (entry == NULL)
  {
    ui_switch_to(SCREEN_EXPR);
    return;
  }

  calc_input_clear(&calc_input);
  while ((position < CALC_INPUT_MAX) && (entry->expression[position] != '\0'))
  {
    (void)calc_input_insert(&calc_input, entry->expression[position]);
    position++;
  }

  window_start = 0U;
  ui_switch_to(SCREEN_EXPR);

  if (evaluate != 0U)
  {
    calc_result_evaluate(&calc_input, &history, 1U, result_line);
  }
}

/**
  * @brief  历史页面：上下翻条目、左右滚长算式、OK 装回输入行。
  * @note   方向键在这里和菜单里不一样：上下是"更旧 / 更新"，左右是横向滚动，
  *         因为一条算式最长 64 格，屏幕一行只有 16 格。
  */
static void handle_history_key(uint8_t index)
{
  const uint8_t count = calc_history_count(&history);

  switch (index)
  {
    case TTP229_KEY_UP:      /* 往更旧的一条翻 */
      if ((uint8_t)(history_index + 1U) < count)
      {
        history_index++;
        history_window = 0U;
      }
      break;

    case TTP229_KEY_DOWN:    /* 往更新的一条翻 */
      if (history_index > 0U)
      {
        history_index--;
        history_window = 0U;
      }
      break;

    case TTP229_KEY_LEFT:
      if (history_window > 0U)
      {
        history_window--;
      }
      break;

    case TTP229_KEY_RIGHT:
      history_window++;      /* 上限在 render 里按算式长度夹住 */
      break;

    case TTP229_KEY_FMT:
      /* 翻历史时也能一键换显示形式，render 会按新格式重画第 2 行。 */
      settings_set_value(SETTING_POLAR,
                         (uint8_t)((settings_value(SETTING_POLAR) == 0U) ? 1U : 0U));
      break;

    case TTP229_KEY_OK:
      history_load(0U);      /* 装进输入行，回去改一改再算 */
      break;

    case TTP229_KEY_EXE:
      history_load(1U);      /* 装进去并立刻重算 */
      break;

    case TTP229_KEY_BACK:
      ui_switch_to(SCREEN_MENU);
      break;

    case TTP229_KEY_MODE:
      ui_switch_to(SCREEN_EXPR);
      break;

    default:
      break;
  }
}

/**
  * @brief  游戏页面的按键。
  * @note   跳跃只认 ↑ / →（OK 留给"重开"，免得玩的时候手忙脚乱按到 OK 就跳）；
  *         撞了以后只有 OK 能重开（同样是防误触），MODE / BACK / AC 一律退出。
  */
static void handle_game_key(uint8_t index)
{
  if (dino_game_is_over(&dino_game) != 0U)
  {
    if (index == TTP229_KEY_OK)
    {
      dino_game_reset(&dino_game);         /* 只有 OK 能重开 */
      next_game_tick = HAL_GetTick();
      game_sky_night = 0xFFU;              /* 重开回到白天，下一个逻辑步刷新天色 */
    }
    else if ((index == TTP229_KEY_MODE) || (index == TTP229_KEY_BACK) ||
             (index == TTP229_KEY_AC))
    {
      ui_switch_to(SCREEN_EXPR);
    }
    return;
  }

  switch (index)
  {
    case TTP229_KEY_UP:
    case TTP229_KEY_RIGHT:
      dino_game_jump(&dino_game);
      break;

    case TTP229_KEY_MODE:
    case TTP229_KEY_BACK:
    case TTP229_KEY_AC:
      ui_switch_to(SCREEN_EXPR);
      break;

    default:
      break;
  }
}

/**
  * @brief  拼一行 "SCORE n RETRY"，放进 16 格（左对齐，右边补空格）。
  * @note   分数最多四位，1~4 位都放得下（"SCORE 1234 RETRY" 正好 16 格）。
  */
static void format_game_over_line(uint16_t score, char line[LCD_COLUMNS])
{
  static const char prefix[] = "SCORE ";
  static const char suffix[] = " RETRY";
  char digits[4];
  uint8_t digit_count = 0U;
  uint8_t position = 0U;
  uint8_t index;

  if (score > 9999U)
  {
    score = 9999U;
  }

  do
  {
    digits[digit_count] = (char)('0' + (char)(score % 10U));
    digit_count++;
    score = (uint16_t)(score / 10U);
  } while ((score > 0U) && (digit_count < 4U));

  view_fill(line, ' ');

  for (index = 0U; (prefix[index] != '\0') && (position < LCD_COLUMNS); ++index)
  {
    line[position] = prefix[index];
    position++;
  }
  while ((digit_count > 0U) && (position < LCD_COLUMNS))
  {
    digit_count--;
    line[position] = digits[digit_count];
    position++;
  }
  for (index = 0U; (suffix[index] != '\0') && (position < LCD_COLUMNS); ++index)
  {
    line[position] = suffix[index];
    position++;
  }
}

/**
  * @brief  把原点写成 "O=x,y" 放进 16 格的一行，放不下就截断。
  */
static void format_origin_line(calc_complex_t origin, char line[LCD_COLUMNS])
{
  char number[CALC_FORMAT_WIDTH + 1U];
  uint8_t length = 0U;
  uint8_t index;

  view_fill(line, ' ');
  line[length++] = 'O';
  line[length++] = '=';
  number[CALC_FORMAT_WIDTH] = '\0';

  calc_format_float(origin.real, number);
  for (index = 0U; (number[index] != '\0') && (number[index] != ' ') &&
                   (length < LCD_COLUMNS); ++index)
  {
    line[length++] = number[index];
  }
  if (length < LCD_COLUMNS)
  {
    line[length++] = ',';
  }
  calc_format_float(origin.imag, number);
  for (index = 0U; (number[index] != '\0') && (number[index] != ' ') &&
                   (length < LCD_COLUMNS); ++index)
  {
    line[length++] = number[index];
  }
}

/**
  * @brief  把结果状态写进文字缓冲：把 "(x,y)" 里的两个数分别求值。
  * @note   两半各自交给求值器，所以原点也能写成 "pi/2"、"1+1" 这样的式子；
  *         但结果必须是实数——原点是平面上的一个点。
  *         空槽（"(,)"、"(1,)"）按语法错处理，不替使用者默认成 0。
  */
static calc_status_t origin_parse(calc_complex_t *value)
{
  char text[CALC_INPUT_MAX + 1U];
  char half[CALC_INPUT_MAX + 1U];
  uint8_t start = 0U;
  uint8_t end = 0U;
  uint8_t index;
  uint8_t comma = 0xFFU;
  uint8_t length;
  calc_complex_t first;
  calc_complex_t second;
  calc_status_t status;

  for (index = 0U; index < calc_input.length; ++index)
  {
    text[index] = calc_input.text[index];
  }
  text[calc_input.length] = '\0';
  end = calc_input.length;

  while ((start < end) && (text[start] == ' '))
  {
    start++;
  }
  while ((end > start) && (text[end - 1U] == ' '))
  {
    end--;
  }
  if ((end > start) && (text[start] == '('))
  {
    start++;
  }
  if ((end > start) && (text[end - 1U] == ')'))
  {
    end--;
  }

  for (index = start; index < end; ++index)
  {
    if (text[index] == ',')
    {
      comma = index;
      break;
    }
  }
  if ((comma == 0xFFU) || (comma == start) || ((uint8_t)(comma + 1U) >= end))
  {
    return CALC_SYNTAX;                    /* 缺一半，或者根本没有逗号 */
  }
  for (index = (uint8_t)(comma + 1U); index < end; ++index)
  {
    if (text[index] == ',')
    {
      return CALC_SYNTAX;                  /* 多出来的逗号 */
    }
  }

  length = (uint8_t)(comma - start);
  for (index = 0U; index < length; ++index)
  {
    half[index] = text[start + index];
  }
  half[length] = '\0';
  status = calculator_evaluate(half,
                               (settings_value(SETTING_ANGLE) == 0U) ? CALC_ANGLE_DEG : CALC_ANGLE_RAD,
                               1U, calc_result_answer(), &first);
  if (status != CALC_OK)
  {
    return status;
  }

  length = (uint8_t)(end - (uint8_t)(comma + 1U));
  for (index = 0U; index < length; ++index)
  {
    half[index] = text[comma + 1U + index];
  }
  half[length] = '\0';
  status = calculator_evaluate(half,
                               (settings_value(SETTING_ANGLE) == 0U) ? CALC_ANGLE_DEG : CALC_ANGLE_RAD,
                               1U, calc_result_answer(), &second);
  if (status != CALC_OK)
  {
    return status;
  }

  if ((first.imag > 1.0e-6f) || (first.imag < -1.0e-6f) ||
      (second.imag > 1.0e-6f) || (second.imag < -1.0e-6f))
  {
    return CALC_DOMAIN;                    /* 原点的两个坐标都必须是实数 */
  }

  value->real = first.real;
  value->imag = second.real;
  return CALC_OK;
}

/**
  * @brief  进入原点输入页面。
  * @note   把算式缓冲整体存起来（退出时还原），换成模板 "(x0,y0)"，
  *         两个槽先填当前原点，所以"不改直接按 OK"就等于沿用原来的原点。
  *         光标停在第一个数末尾：可以直接 DEL 改，按 → 跳到第二个数。
  */
static void origin_begin(uint8_t from_menu)
{
  char first[CALC_FORMAT_WIDTH + 1U];
  char second[CALC_FORMAT_WIDTH + 1U];
  const calc_complex_t current_origin = settings_origin();
  uint8_t index = 0U;

  origin_backup = calc_input;
  origin_backup_window = window_start;
  origin_from_menu = from_menu;
  origin_status = CALC_OK;
  pending_origin = current_origin;

  calc_format_float(current_origin.real, first);
  calc_format_float(current_origin.imag, second);
  first[CALC_FORMAT_WIDTH] = '\0';
  second[CALC_FORMAT_WIDTH] = '\0';

  calc_input_clear(&calc_input);
  (void)calc_input_insert(&calc_input, '(');
  while ((first[index] != '\0') && (first[index] != ' '))
  {
    (void)calc_input_insert(&calc_input, first[index]);
    index++;
  }
  (void)calc_input_insert(&calc_input, ',');
  index = 0U;
  while ((second[index] != '\0') && (second[index] != ' '))
  {
    (void)calc_input_insert(&calc_input, second[index]);
    index++;
  }
  (void)calc_input_insert(&calc_input, ')');

  window_start = 0U;
  ui_switch_to(SCREEN_ORIGIN);
}

/**
  * @brief  离开原点页面：commit = 1 提交（切到极坐标并重画结果），否则丢弃。
  */
static void origin_finish(uint8_t commit)
{
  calc_input = origin_backup;             /* 把算式缓冲还回去 */
  window_start = origin_backup_window;

  if (commit != 0U)
  {
    settings_set_origin(pending_origin);
    settings_set_value(SETTING_POLAR, 1U);   /* 填完原点就连带切到极坐标 */
    calc_result_reapply(&calc_input, result_line);                       /* 立刻按新的原点重画结果 */
    ui_switch_to(SCREEN_EXPR);
  }
  else
  {
    ui_switch_to((origin_from_menu != 0U) ? SCREEN_POLAR : SCREEN_EXPR);
  }
}

/**
  * @brief  原点页面的按键：OK / EXE 提交，BACK / MODE / AC 丢弃，其余照算式界面处理。
  */
static void handle_origin_key(uint8_t index)
{
  calc_complex_t parsed;

  switch (index)
  {
    case TTP229_KEY_OK:
    case TTP229_KEY_EXE:
      origin_status = origin_parse(&parsed);
      if (origin_status == CALC_OK)
      {
        pending_origin = parsed;
        origin_finish(1U);
      }
      break;

    case TTP229_KEY_BACK:
    case TTP229_KEY_MODE:
    case TTP229_KEY_AC:
      origin_finish(0U);
      break;

    case TTP229_KEY_FMT:
      break;                               /* 这个页面里 FMT 不做切换，免得递归进来 */

    default:
      origin_status = CALC_OK;             /* 一动手就把上次的错误提示清掉 */
      handle_expr_key(index);
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
      handle_menu_key(index);
      break;

    case SCREEN_ANGLE:
      handle_option_page(index, SETTING_ANGLE);
      break;

    case SCREEN_COMPLEX:
      handle_option_page(index, SETTING_COMPLEX);
      break;

    case SCREEN_POLAR:
      /* 选中 POLAR 并提交时，先让用户填"原点"，填完再一起生效。 */
      if ((index == TTP229_KEY_OK) && (settings_pending(SETTING_POLAR) != 0U))
      {
        origin_begin(1U);
      }
      else
      {
        handle_option_page(index, SETTING_POLAR);
      }
      break;

    case SCREEN_ORIGIN:
      handle_origin_key(index);
      break;

    case SCREEN_SERIAL:
      handle_view_key(index);
      break;

    case SCREEN_HISTORY:
      handle_history_key(index);
      break;

    case SCREEN_GAME:
      handle_game_key(index);
      break;

    default:
      handle_expr_key(index);
      break;
  }
}

/**
  * @brief  光标左边是否已经是一个完整的操作数。
  * @note   用来决定 x^y 插成 "^()"（底数已输好）还是完整的 "()^()"。
  *         数字、小数点、右括号、常量（pi 的 i、e、Ans 的 A）都算操作数。
  * @retval 1 = 左边是操作数
  */
static uint8_t input_ends_with_operand(void)
{
  char previous;

  if (calc_input.cursor == 0U)
  {
    return 0U;
  }

  previous = calc_input.text[calc_input.cursor - 1U];

  if (((previous >= '0') && (previous <= '9')) || (previous == '.') ||
      (previous == ')') || (previous == 'e') || (previous == 'p') ||
      (previous == 'i') || (previous == 'A'))
  {
    return 1U;
  }

  return 0U;
}


/**
  * @brief  插入一个函数模板："名字()"，并把光标放到括号里面。
  * @note   右括号是模板自带的，所以用户填完参数直接按 EXE 就行；
  *         如果习惯性地再按一次 ')'，只会把光标移过现成的右括号，不会插重复。
  */
static void insert_function_template(const char *name)
{
  while (*name != '\0')
  {
    (void)calc_input_insert(&calc_input, *name);
    name++;
  }

  (void)calc_input_insert(&calc_input, '(');
  (void)calc_input_insert(&calc_input, ')');
  (void)calc_input_move_left(&calc_input);      /* 光标退进括号里 */
}

/* 算式界面用：结果写进第 2 行。 */

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

static void serial_handle_byte(uint8_t byte)
{
  /* 回车或换行：这一行显示完就保留在屏幕上，下次输入时再清掉。 */
  if ((byte == (uint8_t)'\r') || (byte == (uint8_t)'\n'))
  {
    if ((byte == (uint8_t)'\n') && (swallow_next_lf != 0U))
    {
      /* "\r\n" 只当作一次换行。 */
      swallow_next_lf = 0U;
      return;
    }
    swallow_next_lf = (byte == (uint8_t)'\r') ? 1U : 0U;
    serial_length = 0U;
    serial_started = 0U;
    return;
  }

  swallow_next_lf = 0U;

  if (byte == (uint8_t)'\b')
  {
    if (serial_length > 0U)
    {
      serial_length = (uint8_t)(serial_length - 1U);
      serial_line[serial_length] = ' ';
    }
    return;
  }

  if ((byte >= (uint8_t)0x20U) && (byte <= (uint8_t)0x7EU))
  {
    uint8_t index;

    /* 新的一行从清屏开始，避免上一行的残字混进来。 */
    if (serial_started == 0U)
    {
      view_fill(serial_line, ' ');
      serial_started = 1U;
    }

    if (serial_length < LCD_COLUMNS)
    {
      serial_line[serial_length] = (char)byte;
      serial_length = (uint8_t)(serial_length + 1U);
    }
    else
    {
      /* 这一行满了：整行左移一格，始终显示最新收到的字符。 */
      for (index = 0U; index < (LCD_COLUMNS - 1U); ++index)
      {
        serial_line[index] = serial_line[index + 1U];
      }
      serial_line[LCD_COLUMNS - 1U] = (char)byte;
    }
  }
}

static void serial_poll(void)
{
  uint8_t chunk[32];
  uint16_t count;
  uint16_t index;

  count = usb_rx_read(chunk, (uint16_t)sizeof(chunk));
  for (index = 0U; index < count; ++index)
  {
    serial_handle_byte(chunk[index]);
  }
}

/**
  * @brief  生成第 1 行的内容和光标位置。
  */
static void render(void)
{
  top_cursor = NO_CURSOR;
  cursor_row = 0U;

  switch (ui_screen())
  {
    case SCREEN_MENU:
    {
      char item[LCD_COLUMNS + 1U];
      const char *name = menu_item_names[menu_index];
      uint8_t position;

      view_set_text(top_line, "SELECT MODE");

      /* 当前项前面加 '>'，和作者菜单的标记方式一致。 */
      view_fill(item, ' ');
      item[0] = '>';
      for (position = 0U; (position < (LCD_COLUMNS - 2U)) &&
                          (name[position] != '\0'); ++position)
      {
        item[2U + position] = name[position];
      }
      item[LCD_COLUMNS] = '\0';
      view_set_text(bottom_line, item);
      break;
    }

    case SCREEN_ANGLE:
      view_set_text(top_line, "ANGLE UNIT");
      /* 名字在 1 / 11 列，前面一格放标记（'>' 正在选、'*' 已生效） */
      view_option_line(bottom_line, "DEG", 1U, "RAD", 11U,
                       settings_pending(SETTING_ANGLE), settings_value(SETTING_ANGLE));
      break;

    case SCREEN_COMPLEX:
      view_set_text(top_line, "COMPLEX");
      view_option_line(bottom_line, "COMP", 1U, "CMPLX", 11U,
                       settings_pending(SETTING_COMPLEX), settings_value(SETTING_COMPLEX));
      break;

    case SCREEN_POLAR:
      view_set_text(top_line, "POLAR");
      /* 原点不是 (0,0) 时在第 1 行提示一下，免得忘了自己改过 */
      if ((settings_origin().real != 0.0f) || (settings_origin().imag != 0.0f))
      {
        format_origin_line(settings_origin(), top_line);
      }
      view_option_line(bottom_line, "RECT", 1U, "POLAR", 11U,
                       settings_pending(SETTING_POLAR), settings_value(SETTING_POLAR));
      break;

    case SCREEN_SERIAL:
      view_set_text(top_line, "SEND FROM PC");
      (void)memcpy(bottom_line, serial_line, LCD_COLUMNS);
      break;

    case SCREEN_GAME:
      if (dino_game_is_over(&dino_game) != 0U)
      {
        /* 撞了之后：第 1 行 GAME OVER，第 2 行给分数和重开提示 */
        view_set_text(top_line, "GAME OVER");
        format_game_over_line(dino_game.score, bottom_line);
      }
      else
      {
        /* 两行都是游戏画面：上面是云和腾空的恐龙，下面是地面、仙人掌和恐龙 */
        dino_game_render(&dino_game, top_line, bottom_line);
      }
      break;

    case SCREEN_ORIGIN:
      /* 第 1 行是可编辑的 "(x,y)"（带光标），第 2 行是提示或上次的报错 */
      view_format_input(&calc_input, &window_start, top_line, &top_cursor);
      if (origin_status != CALC_OK)
      {
        view_set_text(bottom_line, calc_result_status_text(origin_status));
      }
      else
      {
        view_set_text(bottom_line, "ORIGIN (X,Y)");
      }
      break;

    case SCREEN_HISTORY:
    {
      const calc_history_entry_t *entry = calc_history_get(&history, history_index);

      if (entry == NULL)
      {
        view_set_text(top_line, "HISTORY");
        view_set_text(bottom_line, "EMPTY");
        break;
      }

      /* 第 1 行：这条算式的显示窗口（长算式用左右键横向滚）。 */
      {
        uint8_t length = 0U;
        uint8_t column;
        uint8_t max_window;

        while ((length < CALC_INPUT_MAX) && (entry->expression[length] != '\0'))
        {
          length++;
        }
        /* 窗口最多滑到"最后 16 格"，再往右就整屏空了。 */
        max_window = (length > LCD_COLUMNS) ? (uint8_t)(length - LCD_COLUMNS) : 0U;
        if (history_window > max_window)
        {
          history_window = max_window;
        }

        for (column = 0U; column < LCD_COLUMNS; ++column)
        {
          const uint8_t position = (uint8_t)(history_window + column);

          top_line[column] = (position < length) ? entry->expression[position] : ' ';
        }
      }

      /* 第 2 行：按当前设置格式化这条结果；右边还有空位就补 "n/m" 位置提示。 */
      calc_result_format(entry->result, bottom_line);
      {
        uint8_t length = 0U;

        while ((length < LCD_COLUMNS) && (bottom_line[length] != ' '))
        {
          length++;
        }

        if ((uint8_t)(length + 4U) <= LCD_COLUMNS)
        {
          /* 条目编号和总数都在 8 以内，各占一位。 */
          bottom_line[LCD_COLUMNS - 3U] = (char)('0' + (history_index + 1U));
          bottom_line[LCD_COLUMNS - 2U] = '/';
          bottom_line[LCD_COLUMNS - 1U] = (char)('0' + history.count);
        }
      }
      break;
    }

    default:
      view_format_input(&calc_input, &window_start, top_line, &top_cursor);
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
  calc_input_clear(&calc_input);
  shift_latched = 0U;
  window_start = 0U;
  ui_init();              /* 上电进算式界面 */
  menu_index = 0U;
  settings_init();        /* 角度单位 DEG / 数域 CMPLX / 结果形式 RECT / 原点 (0,0) */
  shadow_valid = 0U;
  (void)memcpy(serial_line, startup_banner, LCD_COLUMNS);
  view_set_text(result_line, "READY");
  serial_length = 0U;
  serial_started = 0U;
  calc_result_init();     /* 清空"上一次结果"（Ans） */
  calc_history_clear(&history);
  history_index = 0U;
  history_window = 0U;
  pending_origin = settings_origin();
  origin_status = CALC_OK;
  dino_game_reset(&dino_game);
  next_game_tick = HAL_GetTick();
  game_sky_night = 0xFFU;

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
      serial_poll();
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

    /* 5. 小游戏：只在游戏界面里推进逻辑，一个逻辑步 70 ms */
    if ((ui_screen() == SCREEN_GAME) && ((int32_t)(now - next_game_tick) >= 0))
    {
      const uint8_t night = dino_game_is_night(&dino_game);

      next_game_tick += DINO_GAME_TICK_MS;
      dino_game_tick(&dino_game);

      /* 白天黑夜每 10 秒交替：天色一变就把 CGRAM 那一格的点阵换掉
         （云 ↔ 星星），屏幕上飘的东西自动跟着变。 */
      if (night != game_sky_night)
      {
        game_sky_night = night;
        lcd_cgram_define_sky(night);
      }
    }
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
