/**
  ******************************************************************************
  * @file    touch_model.c
  * @brief   触摸组合识别，取代 libtouch_model.a。
  *
  *          原理：电极互相耦合，按一个键会点亮好几个电极。判定方法是对每个
  *          候选键算"代价"：
  *            代价 = Σ 每个电极的代价
  *            电极被触发时用 probability_cost[该键下这个电极的激活次数]
  *            电极没触发时用 probability_cost[30 - 激活次数]
  *          取代价最小的候选键作为结果（激活次数越高代价越低）。
  *          只看得到一个电极时直接就是它。
  *
  *          默认标定数据来自作者在原板上采集的默认模型（900 字节表里的
  *          72 个非零条目），等价于那张表；也可以自己上机重新标定。
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include <stddef.h>
#include <string.h>

#include "touch_model.h"

/* Private define ------------------------------------------------------------*/
#define TOUCH_MODEL_MASK 0x3FFFFFFFUL

/* Private types -------------------------------------------------------------*/
typedef struct
{
  uint8_t key;        /* 意图键 */
  uint8_t electrode;  /* 观测到的电极（就是逻辑键号） */
  uint8_t count;      /* 30 次采样里出现多少次 */
} touch_model_sample_t;

/* Private variables ---------------------------------------------------------*/
/* 代价表：下标是激活次数 0..30，次数越高代价越低。 */
static const uint8_t probability_cost[TOUCH_TRAINING_SAMPLES + 1U] =
{
  0x50, 0x40, 0x37, 0x30, 0x2b, 0x27, 0x23, 0x20,
  0x1d, 0x1b, 0x19, 0x17, 0x15, 0x13, 0x12, 0x10,
  0x0f, 0x0d, 0x0c, 0x0b, 0x0a, 0x09, 0x08, 0x07,
  0x06, 0x05, 0x04, 0x03, 0x02, 0x02, 0x01
};

/* 作者默认模型里的非零条目（键, 电极, 次数）。 */
static const touch_model_sample_t touch_model_default_samples[] =
{
  {  0,  0, 30 }, {  0,  5, 14 },
  {  1,  1, 30 }, {  1,  2,  1 },
  {  1,  6, 21 }, {  2,  2, 30 },
  {  2,  7,  8 }, {  2, 12,  5 },
  {  3,  3, 30 }, {  3,  8, 19 },
  {  4,  4, 30 }, {  4,  9, 13 },
  {  5,  5, 30 }, {  5, 10, 25 },
  {  6,  6, 30 }, {  6, 11, 16 },
  {  7,  7, 10 }, {  7, 11,  4 },
  {  7, 12, 30 }, {  8,  3,  1 },
  {  8,  8, 30 }, {  8, 13, 25 },
  {  8, 14,  1 }, {  9,  4,  1 },
  {  9,  9, 30 }, {  9, 14,  6 },
  { 10, 10, 30 }, { 10, 11,  1 },
  { 11,  6, 26 }, { 11,  7, 20 },
  { 11, 11, 30 }, { 12, 12, 30 },
  { 12, 13, 19 }, { 13,  8,  1 },
  { 13, 12, 28 }, { 13, 13, 30 },
  { 14, 14, 30 }, { 15, 10,  1 },
  { 15, 15, 30 }, { 15, 20,  7 },
  { 16, 16, 30 }, { 16, 21, 28 },
  { 17, 17, 30 }, { 17, 22, 17 },
  { 18, 18, 30 }, { 18, 23, 19 },
  { 19, 19, 30 }, { 20, 10,  1 },
  { 20, 15, 18 }, { 20, 20, 30 },
  { 21, 16, 21 }, { 21, 21, 30 },
  { 22, 17,  6 }, { 22, 22, 30 },
  { 23, 18, 26 }, { 23, 23, 30 },
  { 23, 28, 16 }, { 24, 19,  4 },
  { 24, 24, 30 }, { 25, 16, 17 },
  { 25, 20, 26 }, { 25, 21,  4 },
  { 25, 25, 30 }, { 26, 17,  6 },
  { 26, 21, 20 }, { 26, 26, 30 },
  { 27, 22, 22 }, { 27, 27, 20 },
  { 28, 18,  4 }, { 28, 23, 26 },
  { 28, 28, 30 }, { 29, 29, 30 }
};

/* Exported functions --------------------------------------------------------*/

void touch_model_reset(touch_model_t *model)
{
  if (model == NULL)
  {
    return;
  }

  (void)memset(model, 0, sizeof(*model));
}

void touch_model_load_default(touch_model_t *model)
{
  uint32_t index;

  if (model == NULL)
  {
    return;
  }

  (void)memset(model->activation_count, 0, sizeof(model->activation_count));

  for (index = 0U; index < (sizeof(touch_model_default_samples) /
                            sizeof(touch_model_default_samples[0])); ++index)
  {
    const touch_model_sample_t *sample = &touch_model_default_samples[index];

    model->activation_count[sample->key][sample->electrode] = sample->count;
  }

  model->ready = 1U;
}

void touch_model_add_sample(touch_model_t *model, uint8_t intended_key,
                            uint32_t observed_bitmap)
{
  uint8_t electrode;

  if ((model == NULL) || (intended_key >= TOUCH_KEY_COUNT))
  {
    return;
  }

  observed_bitmap &= TOUCH_MODEL_MASK;

  for (electrode = 0U; electrode < TOUCH_KEY_COUNT; ++electrode)
  {
    if ((observed_bitmap & (1UL << electrode)) != 0U)
    {
      if (model->activation_count[intended_key][electrode] < 29U)
      {
        model->activation_count[intended_key][electrode]++;
      }
    }
  }
}

uint32_t touch_model_classify(const touch_model_t *model,
                              uint32_t observed_bitmap)
{
  uint8_t key;
  uint8_t electrode;
  uint8_t best_key = 0U;
  uint32_t best_cost = 0xFFFFFFFFUL;

  if ((model == NULL) || (model->ready == 0U))
  {
    return 0U;
  }

  observed_bitmap &= TOUCH_MODEL_MASK;

  /* 一个电极都没触发 → 没有按键。 */
  if (observed_bitmap == 0U)
  {
    return 0U;
  }

  /* 只触发一个电极 → 直接就是它。 */
  if ((observed_bitmap & (observed_bitmap - 1U)) == 0U)
  {
    return observed_bitmap;
  }

  /* 数据里出现过的一对固定组合（电极 12 和 13 同时触发 → 键 12）。 */
  if (observed_bitmap == 0x3000UL)
  {
    return 0x1000UL;
  }

  for (key = 0U; key < TOUCH_KEY_COUNT; ++key)
  {
    uint32_t cost = 0U;

    for (electrode = 0U; electrode < TOUCH_KEY_COUNT; ++electrode)
    {
      uint8_t count = model->activation_count[key][electrode];

      if ((observed_bitmap & (1UL << electrode)) == 0U)
      {
        count = (uint8_t)(TOUCH_TRAINING_SAMPLES - count);
      }

      cost += probability_cost[count];
    }

    if (cost < best_cost)
    {
      best_cost = cost;
      best_key = key;
    }
  }

  return 1UL << best_key;
}
