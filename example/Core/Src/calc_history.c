/**
  ******************************************************************************
  * @file    calc_history.c
  * @brief   算式历史记录（环形缓冲）：存算式和它的结果，按键那边翻着看。
  *
  *          只存数据，不格式化也不画屏——显示交给 main.c，格式化交给
  *          calc_format.c，这样在 PC 上可以单独编译测试。
  *
  *          满了以后覆盖最旧的：head 一直指向最新一条，取第 index 条就是
  *          从 head 往回数 index 格。
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include <stddef.h>

#include "calc_history.h"

/* Exported functions --------------------------------------------------------*/

void calc_history_clear(calc_history_t *history)
{
  history->count = 0U;
  history->head = 0U;
}

void calc_history_push(calc_history_t *history, const char *expression,
                       calc_complex_t result)
{
  uint8_t index = 0U;

  if ((expression == NULL) || (expression[0] == '\0'))
  {
    return;
  }

  history->head = (uint8_t)((history->head + 1U) % CALC_HISTORY_DEPTH);

  while ((index < CALC_INPUT_MAX) && (expression[index] != '\0'))
  {
    history->entry[history->head].expression[index] = expression[index];
    index++;
  }
  history->entry[history->head].expression[index] = '\0';
  history->entry[history->head].result = result;

  if (history->count < CALC_HISTORY_DEPTH)
  {
    history->count++;
  }
}

uint8_t calc_history_count(const calc_history_t *history)
{
  return history->count;
}

const calc_history_entry_t *calc_history_get(const calc_history_t *history,
                                             uint8_t index)
{
  if (index >= history->count)
  {
    return NULL;
  }

  return &history->entry[(uint8_t)((history->head + CALC_HISTORY_DEPTH - index) %
                                   CALC_HISTORY_DEPTH)];
}
