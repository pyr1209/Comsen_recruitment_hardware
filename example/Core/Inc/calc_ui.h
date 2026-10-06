#ifndef CALC_UI_H
#define CALC_UI_H

#include <stdint.h>

/*
 * 界面状态机：记录"现在在哪个页面"，并提供唯一的切页入口。
 *
 * 这一步只搬"状态部分"：变量本体藏在 calc_ui.c 里，外面只能通过
 * ui_screen() 读、ui_switch_to() 申请切换 —— 于是任何页面文件都不需要
 * "extern 别人的变量"，只要 include 这个头就能拿到类型和两个函数。
 *
 * 按键分发（switch (screen) 那一大段）等各个页面都搬成独立文件之后再进来，
 * 那时它只需要调用各页面提供的 xxx_handle_key()。
 */

typedef enum
{
  SCREEN_EXPR = 0,     /* 算式界面：第 1 行算式，第 2 行结果 */
  SCREEN_MENU,         /* SELECT MODE 列表 */
  SCREEN_ANGLE,        /* ANGLE UNIT：DEG / RAD */
  SCREEN_COMPLEX,      /* COMPLEX：COMP / CMPLX */
  SCREEN_POLAR,        /* POLAR：RECT / POLAR */
  SCREEN_SERIAL,       /* 查看电脑发来的内容 */
  SCREEN_HISTORY,      /* 翻看算过的算式和结果 */
  SCREEN_GAME,         /* 小恐龙跳仙人掌 */
  SCREEN_ORIGIN        /* 输入极坐标显示的"原点" */
} ui_screen_t;

/* 上电进入算式界面。 */
void ui_init(void);

/* 现在在哪个页面。 */
ui_screen_t ui_screen(void);

/* 切到某个页面（唯一入口）。 */
void ui_switch_to(ui_screen_t next);

#endif
