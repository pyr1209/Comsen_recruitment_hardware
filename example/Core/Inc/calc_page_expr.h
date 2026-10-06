#ifndef CALC_PAGE_EXPR_H
#define CALC_PAGE_EXPR_H

#include <stdint.h>

#include "calc_input.h"
#include "calc_view.h"

/*
 * 算式页面：屏幕第 1 行那个可编辑的算式。
 *
 * 它管三样东西——输入缓冲本体、长算式的显示窗口、以及"哪个键插入什么字符"
 * 的键表（含 SHIFT 上档层）。原来这些散在 main.c 的 handle_expr_key /
 * input_ends_with_operand / insert_function_template 和 3 个 static 变量里。
 *
 * 会牵动别的模块的事（打开菜单、求值、清结果、按新形式重画结果）不在这里做，
 * 而是作为"动作"返回给调用方 —— 于是本模块只依赖输入缓冲和显示层工具，
 * 不认识求值器、历史、界面状态机，能在 PC 上单独编译测试。
 */

/* 处理一个键之后要调用方补做的事。 */
typedef enum
{
  EXPR_ACTION_NONE = 0,        /* 页面内部处理完了（插字符、移光标、清行……） */
  EXPR_ACTION_OPEN_MENU,       /* MODE：请打开模式菜单 */
  EXPR_ACTION_EVALUATE,        /* EXE：请求值并把结果写进结果行 */
  EXPR_ACTION_CLEAR_RESULT,    /* AC：请把结果行拉回 READY 并清掉 Ans */
  EXPR_ACTION_REFRESH_RESULT   /* FMT：请按新的结果形式重画上一次结果 */
} expr_action_t;

/* 上电：清空算式、窗口和 SHIFT 上档。 */
void expr_page_init(void);

/* 输入缓冲本体：历史页装入算式、按 EXE 求值都要用它。 */
calc_input_t *expr_page_input(void);

/* 从历史页装入算式之后调用：把显示窗口拉回最左边。 */
void expr_page_window_reset(void);

/* 处理一个键；shifted = 1 表示这一下是按住 SHIFT 之后按的（走上档层）。
   SHIFT 键本身由调用方处理（它只是翻转锁存，不进这里）。 */
expr_action_t expr_page_handle_key(uint8_t key, uint8_t shifted);

/* 画算式的第 1 行，并给出光标所在列（没有光标时写 CALC_VIEW_NO_CURSOR）。 */
void expr_page_render(char line[VIEW_LINE_WIDTH], uint8_t *cursor_column);

#endif
