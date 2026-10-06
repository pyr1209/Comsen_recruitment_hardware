#ifndef CALC_HISTORY_H
#define CALC_HISTORY_H

#include <stdint.h>

#include "calc_input.h"
#include "calculator_engine.h"

/* 最多记多少条。环形缓冲：写满之后新的覆盖最旧的。 */
#define CALC_HISTORY_DEPTH 8U

typedef struct
{
  char           expression[CALC_INPUT_MAX + 1U];  /* 当时的算式，'\0' 结尾 */
  calc_complex_t result;                           /* 当时算出来的结果 */
} calc_history_entry_t;

typedef struct
{
  calc_history_entry_t entry[CALC_HISTORY_DEPTH];
  uint8_t              count;   /* 已存条数，0..CALC_HISTORY_DEPTH */
  uint8_t              head;    /* 最新一条所在的槽位 */
} calc_history_t;

/* 本模块只管"记什么、怎么取"，不碰屏幕和按键，方便在 PC 上单独测。 */
void calc_history_clear(calc_history_t *history);

/* 记一条。算式为空时不记（Syntax ERROR 那种没有记录价值）。 */
void calc_history_push(calc_history_t *history, const char *expression,
                       calc_complex_t result);

uint8_t calc_history_count(const calc_history_t *history);

/* index = 0 是最新一条，1 是上一条……越界返回 NULL。 */
const calc_history_entry_t *calc_history_get(const calc_history_t *history,
                                             uint8_t index);

#endif
