/**
  ******************************************************************************
  * @file    calc_app.c
  * @brief   应用层：按键分派、渲染分派、共享状态（历史 / 结果行 / SHIFT）。
  *
  *          原来这些是 main.c 里的 ui_handle_key、handle_expr_key、render 三个
  *          switch，外加 history / result_line / shift_latched 等 static 变量
  *          和一大段初始化。搬到这里之后 main.c 只剩下：
  *              芯片与外设初始化 → app_init() → while (1) 里按节拍调 app_xxx()
  *
  *          为什么不做进 calc_ui：页面要用 ui_switch_to()（页面 → calc_ui），
  *          如果 calc_ui 再反过来 include 所有页面，两者就互相依赖了。
  *          所以 calc_ui 继续当"只认屏幕枚举"的纯状态机，装配与分派放在这一层。
  *
  *          依赖方向：calc_app → 各页面 → calc_ui / calc_settings / …
  *          页面之间不互相依赖（页面要动别人的东西时返回"动作"，由这里落实）。
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "calc_app.h"

#include <string.h>

#include "calc_history.h"
#include "calc_keymap.h"
#include "calc_page_expr.h"
#include "calc_page_game.h"
#include "calc_page_history.h"
#include "calc_page_menu.h"
#include "calc_page_serial.h"
#include "calc_result.h"
#include "calc_settings.h"
#include "calc_ui.h"

/* Private variables ---------------------------------------------------------*/
static calc_history_t history;              /* 算式历史（最近 8 条） */
static char result_line[VIEW_LINE_WIDTH];   /* 结果行：由 calc_result 生成 */
static uint8_t shift_latched;               /* 上档锁存：结果行右端亮 'S' */
static uint8_t shift_applied;               /* 本次按下的键是否走上档层 */

/* Private function prototypes -----------------------------------------------*/
static void handle_expr_key(uint8_t key);

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  算式界面：把页面返回的"动作"落到实处。
  * @note   页面自己管输入缓冲、显示窗口和键表（calc_page_expr.c），
  *         牵动别的模块的事——开菜单、求值、清结果、重画结果——在这里做。
  */
static void handle_expr_key(uint8_t key)
{
  switch (expr_page_handle_key(key, shift_applied))
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

/* Exported functions --------------------------------------------------------*/

void app_init(void)
{
  shift_latched = 0U;
  shift_applied = 0U;

  ui_init();              /* 上电进算式界面 */
  expr_page_init();
  menu_page_init();
  settings_init();        /* 角度单位 DEG / 数域 CMPLX / 结果形式 RECT */
  serial_page_init();
  calc_result_init();     /* 清空"上一次结果"（Ans） */
  calc_history_clear(&history);
  history_page_reset();
  game_page_init();

  view_set_text(result_line, "READY");
}

void app_handle_key(uint8_t key)
{
  /* SHIFT 是输入层的上档锁存：按一下翻转；按别的键时"消费"掉这一下，
     于是上档只影响紧跟着的那一次按键（在哪个界面都会消费掉）。 */
  if (key == TTP229_KEY_SHIFT)
  {
    shift_latched = (uint8_t)(shift_latched ^ 1U);
    shift_applied = 0U;
  }
  else
  {
    shift_applied = shift_latched;
    if (shift_latched != 0U)
    {
      shift_latched = 0U;
    }
  }

  switch (ui_screen())
  {
    case SCREEN_MENU:
      switch (menu_page_handle_key(key))
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

      switch (option_page_handle_key(key, id))
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
      serial_page_handle_key(key);
      break;

    case SCREEN_HISTORY:
    {
      /* 页面返回"要不要把某条装回输入行"；装入和重算是应用层的事，在这里做。 */
      const history_action_t action = history_page_handle_key(key, &history);

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
      game_page_handle_key(key);
      break;

    default:
      handle_expr_key(key);
      break;
  }
}

void app_poll_serial(void)
{
  serial_page_poll();
}

void app_poll_game(void)
{
  game_page_poll();
}

void app_render(char top[VIEW_LINE_WIDTH], char bottom[VIEW_LINE_WIDTH],
                uint8_t *cursor_column, uint8_t *cursor_row)
{
  *cursor_column = CALC_VIEW_NO_CURSOR;
  *cursor_row = 0U;

  switch (ui_screen())
  {
    case SCREEN_MENU:
      menu_page_render(top, bottom);
      break;

    case SCREEN_ANGLE:
    case SCREEN_COMPLEX:
    case SCREEN_POLAR:
    {
      const setting_id_t id = (ui_screen() == SCREEN_ANGLE) ? SETTING_ANGLE
                            : ((ui_screen() == SCREEN_COMPLEX) ? SETTING_COMPLEX
                                                               : SETTING_POLAR);

      option_page_render(id, top, bottom);
      break;
    }

    case SCREEN_SERIAL:
      serial_page_render(top, bottom);
      break;

    case SCREEN_GAME:
      game_page_render(top, bottom);
      break;

    case SCREEN_HISTORY:
      history_page_render(&history, top, bottom);
      break;

    default:
      expr_page_render(top, cursor_column);
      (void)memcpy(bottom, result_line, VIEW_LINE_WIDTH);

      /* 上档锁存时在结果行最右端亮一个 'S'：提示下一个字符键会走 SHIFT 层。
         结果文本最长也就十来个字符，右端这几格是空的，不会挡住结果。 */
      if (shift_latched != 0U)
      {
        bottom[VIEW_LINE_WIDTH - 1U] = 'S';
      }
      break;
  }
}
