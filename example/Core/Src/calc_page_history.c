/**
  ******************************************************************************
  * @file    calc_page_history.c
  * @brief   历史记录页面：翻看最近 8 条算式与结果，并能把某条装回输入行。
  *
  *          原来这一页的代码散在 main.c 里（history_load / handle_history_key
  *          / render 的 HISTORY 分支 + 2 个 static 状态）。现在整页搬到这里。
  *
  *          设计上有个刻意的选择：页面**不直接动输入缓冲、不调求值器**，而是
  *          在 OK / EXE 时返回一个"动作"，由调用方（main.c）去执行
  *          "装入输入行、可选重算一次"。这样页面只依赖历史数据本身，
  *          不需要知道结果行长什么样、也不需要认识求值流程（依赖保持单向）。
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "calc_page_history.h"

#include <stddef.h>

#include "calc_keymap.h"
#include "calc_result.h"
#include "calc_settings.h"
#include "calc_ui.h"

/* Private variables ---------------------------------------------------------*/
static uint8_t history_index;    /* 正在看第几条：0 = 最新 */
static uint8_t history_window;   /* 长算式的横向显示窗口 */

/* Exported functions --------------------------------------------------------*/

void history_page_reset(void)
{
  history_index = 0U;            /* 进来先看最新一条 */
  history_window = 0U;
}

uint8_t history_page_load(const calc_history_t *history, calc_input_t *input)
{
  const calc_history_entry_t *entry = calc_history_get(history, history_index);
  uint8_t position = 0U;

  if (entry == NULL)
  {
    return 0U;
  }

  calc_input_clear(input);
  while ((position < CALC_INPUT_MAX) && (entry->expression[position] != '\0'))
  {
    (void)calc_input_insert(input, entry->expression[position]);
    position++;
  }

  return 1U;
}


history_action_t history_page_handle_key(uint8_t key, const calc_history_t *history)
{
  const uint8_t count = calc_history_count(history);

  switch (key)
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
      /* 装进输入行，回去改一改再算；具体动作交给调用方执行。 */
      if (calc_history_get(history, history_index) != NULL)
      {
        ui_switch_to(SCREEN_EXPR);
        return HISTORY_ACTION_LOAD;
      }
      ui_switch_to(SCREEN_EXPR);
      break;

    case TTP229_KEY_EXE:
      /* 装进去并立刻重算一遍。 */
      if (calc_history_get(history, history_index) != NULL)
      {
        ui_switch_to(SCREEN_EXPR);
        return HISTORY_ACTION_LOAD_EVAL;
      }
      ui_switch_to(SCREEN_EXPR);
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
  return HISTORY_ACTION_NONE;
}


void history_page_render(const calc_history_t *history,
                         char top[VIEW_LINE_WIDTH], char bottom[VIEW_LINE_WIDTH])
{
  const calc_history_entry_t *entry = calc_history_get(history, history_index);

  if (entry == NULL)
  {
    view_set_text(top, "HISTORY");
    view_set_text(bottom, "EMPTY");
    return;
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
    max_window = (length > VIEW_LINE_WIDTH) ? (uint8_t)(length - VIEW_LINE_WIDTH) : 0U;
    if (history_window > max_window)
    {
      history_window = max_window;
    }

    for (column = 0U; column < VIEW_LINE_WIDTH; ++column)
    {
      const uint8_t position = (uint8_t)(history_window + column);

      top[column] = (position < length) ? entry->expression[position] : ' ';
    }
  }

  /* 第 2 行：按当前设置格式化这条结果；右边还有空位就补 "n/m" 位置提示。 */
  calc_result_format(entry->result, bottom);
  {
    uint8_t length = 0U;

    while ((length < VIEW_LINE_WIDTH) && (bottom[length] != ' '))
    {
      length++;
    }

    if ((uint8_t)(length + 4U) <= VIEW_LINE_WIDTH)
    {
      /* 条目编号和总数都在 8 以内，各占一位。 */
      bottom[VIEW_LINE_WIDTH - 3U] = (char)('0' + (history_index + 1U));
      bottom[VIEW_LINE_WIDTH - 2U] = '/';
      bottom[VIEW_LINE_WIDTH - 1U] = (char)('0' + history->count);
    }
  }
}
