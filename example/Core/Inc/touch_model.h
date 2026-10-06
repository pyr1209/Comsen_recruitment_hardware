#ifndef TOUCH_MODEL_H
#define TOUCH_MODEL_H

#include <stdint.h>

#define TOUCH_KEY_COUNT 30U
#define TOUCH_TRAINING_SAMPLES 30U

/*
 * 触摸组合模型：把"观测到的电极集合"还原成"用户意图的那个键"。
 *
 * 这块板的电极之间会互相耦合：按一个键通常会同时触发 2~4 个电极
 * （作者采集的默认标定数据里，30 个键有 27 个都是这样）。
 * 所以不能拿原始位图直接当按键用，要先经过本模型判定。
 *
 * 本文件与 touch_model.c 是自己实现的，接口与作者提供的 libtouch_model.a 一致。
 */
typedef struct
{
  uint8_t activation_count[TOUCH_KEY_COUNT][TOUCH_KEY_COUNT];
  uint8_t ready;
} touch_model_t;

void touch_model_reset(touch_model_t *model);
void touch_model_load_default(touch_model_t *model);
void touch_model_add_sample(touch_model_t *model, uint8_t intended_key,
                            uint32_t observed_bitmap);

/* 返回 0 或单个按键位 1<<键号。 */
uint32_t touch_model_classify(const touch_model_t *model,
                              uint32_t observed_bitmap);

#endif
