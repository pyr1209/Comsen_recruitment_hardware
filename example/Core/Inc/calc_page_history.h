#ifndef CALC_PAGE_HISTORY_H
#define CALC_PAGE_HISTORY_H

#include <stdint.h>

#include "calc_history.h"
#include "calc_input.h"
#include "calc_view.h"

/*
 * 历史记录页面：翻看最近 8 条，并支持把某条装回输入行。
 *
 * 页面自己管"正在看第几条 / 横向窗口"，指令式的动作（装入输入行、要不要重算）
 * 通过返回值交给调用方执行 —— 所以它不认识结果行，也不调求值器。
 */

/* 页面按键的结果：调用方据此决定要不要做"装入/重算"。 */
typedef enum
{
  HISTORY_ACTION_NONE = 0,     /* 页面内部处理完了（翻页 / 滚窗 / 切页 / 换显示形式） */
  HISTORY_ACTION_LOAD,         /* 请把选中的算式装进输入行 */
  HISTORY_ACTION_LOAD_EVAL     /* 请装入并立刻重算一次 */
} history_action_t;

/* 进页面：先看最新一条，窗口归零。 */
void history_page_reset(void);

/* 按键：翻条目、滚窗口、换显示形式、退出；OK / EXE 返回上面的动作。 */
history_action_t history_page_handle_key(uint8_t key, const calc_history_t *history);

/* 把当前选中那条的算式装进 input；返回 1 = 装进去了（空历史返回 0）。 */
uint8_t history_page_load(const calc_history_t *history, calc_input_t *input);

/* 画这个页面的两行：第 1 行算式，第 2 行按当前设置格式化的结果。 */
void history_page_render(const calc_history_t *history,
                         char top[VIEW_LINE_WIDTH], char bottom[VIEW_LINE_WIDTH]);

#endif
