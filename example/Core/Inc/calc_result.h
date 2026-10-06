#ifndef CALC_RESULT_H
#define CALC_RESULT_H

#include <stdint.h>

#include "calc_history.h"
#include "calc_input.h"
#include "calc_view.h"
#include "calculator_engine.h"

/*
 * 结果流程：算式 → 求值 → 按当前设置格式化 → 写结果行，并记住上一次的结果（Ans）。
 *
 * 实例（输入缓冲、历史）都由调用方持有并当参数传进来，本模块只保留自己那份
 * "上一次结果"。所以它不 include HAL，也能在 PC 上单测整条链路。
 */

/* 上电清空"上一次结果"。 */
void calc_result_init(void);

/* 按 EXE：求值并把结果/错误写进 line；remember = 1 时把成功的算式记进历史。 */
void calc_result_evaluate(const calc_input_t *input, calc_history_t *history,
                          uint8_t remember, char line[VIEW_LINE_WIDTH]);

/* 设置提交后的"重新应用"：算式非空就重算，空着就把上一次结果按新设置重画。 */
void calc_result_reapply(const calc_input_t *input, char line[VIEW_LINE_WIDTH]);

/* 把上一次结果按当前设置重画（FMT 切回直角坐标时用）；返回 1 = 确实有结果。 */
uint8_t calc_result_show_answer(char line[VIEW_LINE_WIDTH]);

/* 把一个结果值格式化成一行（历史页显示某一条时用）。 */
void calc_result_format(calc_complex_t value, char line[VIEW_LINE_WIDTH]);

/* 求值状态码 → 给用户看的提示文字。 */
const char *calc_result_status_text(calc_status_t status);

/* "上一次结果"（Ans）：算式的 A 用它，AC 时清掉。 */
calc_complex_t calc_result_answer(void);
void           calc_result_clear_answer(void);

#endif
