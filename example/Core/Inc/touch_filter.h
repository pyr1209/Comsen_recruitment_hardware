#ifndef TOUCH_FILTER_H
#define TOUCH_FILTER_H

#include <stdint.h>

/*
 * 按键去抖与输入处理。本头文件和 touch_filter.c 是自己实现的，
 * 取代作者提供的 libtouch_filter.a：前四个字段与原结构体保持同名同序，
 * 后四个字段是自研实现新增的按键事件与按住时长，供后面的输入处理使用。
 *
 * 约定：touch_filter_update() 每 10 ms 调用一次。
 */
typedef struct
{
  uint8_t counters[30];        /* 每个键的连续一致采样计数，0..3 */
  uint32_t stable_bitmap;      /* 已经去抖稳定的按下集合 */
  uint32_t active_bitmap;      /* 正在上报的那个键（0 表示空闲） */
  uint8_t wait_all_released;   /* 上次上报后是否还在等待全部松开 */

  /* —— 以下为自研实现新增 —— */
  uint32_t previous_bitmap;    /* 上一次 update 结束时的稳定集合，用于求边沿 */
  uint32_t pressed_events;     /* 本次 update 新按下的键（上升沿） */
  uint32_t released_events;    /* 本次 update 新松开的键（下降沿） */
  uint8_t hold_ticks[30];      /* 已稳定按住的周期数，10 ms 一格，255 饱和 */
} touch_filter_t;

void touch_filter_init(touch_filter_t *filter);

/* Returns either zero or one physical key bit. Call every 10 ms. */
uint32_t touch_filter_update(touch_filter_t *filter, uint32_t raw_bitmap);

/* 按住时长的判定阈值：hold_ticks 达到该值即视为长按（100 * 10 ms = 1 s）。 */
#define TOUCH_FILTER_LONG_PRESS_TICKS 100U

static inline uint8_t touch_filter_is_long_press(const touch_filter_t *filter,
                                                 uint8_t key_index)
{
  return (uint8_t)(filter->hold_ticks[key_index] >= TOUCH_FILTER_LONG_PRESS_TICKS);
}

#endif
