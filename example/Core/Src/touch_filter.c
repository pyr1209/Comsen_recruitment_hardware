/**
  ******************************************************************************
  * @file    touch_filter.c
  * @brief   自己实现的按键去抖与输入处理，取代 libtouch_filter.a。
  *
  *          行为与原库一致，逐条对应：
  *            1. 每 10 ms 调用一次，逐位做"连续 3 次一致才改变"的计数式去抖；
  *            2. 去抖后的结果放在 stable_bitmap；
  *            3. 一次按键只上报一次（active_bitmap），并且必须等所有键都
  *               松开（wait_all_released）才允许下一次上报；
  *            4. 返回值是 0 或单个按键位 1<<键号。
  *
  *          在此基础上增加了按键事件（按下/松开边沿）和按住时长，
  *          供后面的输入缓冲、长按等功能使用。
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include <stddef.h>

#include "touch_filter.h"

/* Private define ------------------------------------------------------------*/
#define TOUCH_FILTER_KEY_COUNT   30U
#define TOUCH_FILTER_KEY_MASK    0x3FFFFFFFUL  /* 只用低 30 位，忽略 bit30/31 */
#define TOUCH_FILTER_THRESHOLD   3U            /* 连续 3 次一致才算数（30 ms） */
#define TOUCH_FILTER_HOLD_MAX    255U          /* 按住时长饱和值 */

/* Exported functions --------------------------------------------------------*/

void touch_filter_init(touch_filter_t *filter)
{
  uint8_t index;

  if (filter == NULL)
  {
    return;
  }

  for (index = 0U; index < TOUCH_FILTER_KEY_COUNT; ++index)
  {
    filter->counters[index] = 0U;
    filter->hold_ticks[index] = 0U;
  }

  filter->stable_bitmap = 0U;
  filter->active_bitmap = 0U;
  filter->wait_all_released = 0U;
  filter->previous_bitmap = 0U;
  filter->pressed_events = 0U;
  filter->released_events = 0U;
}

uint32_t touch_filter_update(touch_filter_t *filter, uint32_t raw_bitmap)
{
  uint32_t bit;
  uint8_t index;

  if (filter == NULL)
  {
    return 0U;
  }

  raw_bitmap &= TOUCH_FILTER_KEY_MASK;

  /* 1) 计数式去抖：连续 3 次按下才置位，连续 3 次松开才清位。
        中途抖动只会让计数来回加减，不会立刻改变稳定状态。 */
  for (index = 0U; index < TOUCH_FILTER_KEY_COUNT; ++index)
  {
    bit = 1UL << index;

    if ((raw_bitmap & bit) != 0U)
    {
      if (filter->counters[index] < TOUCH_FILTER_THRESHOLD)
      {
        filter->counters[index]++;
      }
      if (filter->counters[index] >= TOUCH_FILTER_THRESHOLD)
      {
        filter->stable_bitmap |= bit;
      }
    }
    else
    {
      if (filter->counters[index] > 0U)
      {
        filter->counters[index]--;
      }
      if (filter->counters[index] == 0U)
      {
        filter->stable_bitmap &= ~bit;
      }
    }

    /* 按住时长：只统计已经稳定的按住，松开即清零。 */
    if ((filter->stable_bitmap & bit) != 0U)
    {
      if (filter->hold_ticks[index] < TOUCH_FILTER_HOLD_MAX)
      {
        filter->hold_ticks[index]++;
      }
    }
    else
    {
      filter->hold_ticks[index] = 0U;
    }
  }

  /* 2) 按键事件：与上一次的稳定集合比较，得到按下/松开边沿。 */
  filter->pressed_events = filter->stable_bitmap & ~filter->previous_bitmap;
  filter->released_events = filter->previous_bitmap & ~filter->stable_bitmap;
  filter->previous_bitmap = filter->stable_bitmap;

  /* 3) 上报门控：一个键先上报一次，并且要等全部松开才允许下一次。 */
  if (filter->active_bitmap != 0U)
  {
    if ((filter->stable_bitmap & filter->active_bitmap) == 0U)
    {
      /* 被上报的键已经松开：结束本次上报，开始等待全部松开。 */
      filter->active_bitmap = 0U;
      filter->wait_all_released = 1U;
    }
    return 0U;
  }

  if (filter->wait_all_released != 0U)
  {
    if (filter->stable_bitmap != 0U)
    {
      return 0U;
    }
    filter->wait_all_released = 0U;
  }

  /* 从低位开始取第一个按下的键上报。 */
  for (index = 0U; index < TOUCH_FILTER_KEY_COUNT; ++index)
  {
    bit = 1UL << index;
    if ((filter->stable_bitmap & bit) != 0U)
    {
      filter->active_bitmap = bit;
      return bit;
    }
  }

  return 0U;
}
