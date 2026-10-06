/**
  ******************************************************************************
  * @file    calc_ui.c
  * @brief   界面状态机的"状态部分"：当前在哪个页面。
  *
  *          screen 原来是 main.c 里的 static 全局，谁都能直接赋值（20 处）。
  *          现在它的真身藏在这里，外面只能 ui_screen() 读、ui_switch_to() 切，
  *          这样页面文件搬出去时不需要 extern 别人的变量。
  *
  *          按键分发（switch (screen) 那一大段）等页面都独立之后再搬进来。
  *          本文件不 include HAL，可以在 PC 上单测。
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "calc_ui.h"

/* Private variables ---------------------------------------------------------*/
static ui_screen_t ui_screen_state;      /* 当前页面 */

/* Exported functions --------------------------------------------------------*/

void ui_init(void)
{
  ui_screen_state = SCREEN_EXPR;         /* 上电进算式界面 */
}

ui_screen_t ui_screen(void)
{
  return ui_screen_state;
}

void ui_switch_to(ui_screen_t next)
{
  ui_screen_state = next;
}
