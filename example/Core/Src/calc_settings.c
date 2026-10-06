/**
  ******************************************************************************
  * @file    calc_settings.c
  * @brief   三个全局设置项（角度单位 / 数域 / 结果形式）和极坐标原点。
  *
  *          每个设置项两份值：生效值 + 暂存值。进设置页先"暂存 ← 生效"，
  *          方向键只改暂存，OK 提交、BACK / MODE 丢弃。这套语义原来散在
  *          main.c 的 handle_option_key 和各个页面的进入/退出里，现在收在这里，
  *          页面只负责画和导航。
  *
  *          本文件不 include HAL、不读任何全局、也不认识键号 → 可以在 PC 上单测。
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "calc_settings.h"

/* Private variables ---------------------------------------------------------*/
/* 生效值：求值和显示实际用的 0/1；暂存值：设置页里临时改的那个。 */
static uint8_t  setting_value[SETTING_COUNT];
static uint8_t  setting_pending[SETTING_COUNT];

/* 极坐标显示的"原点"：结果先减去它，再写成 r∠θ。默认 (0,0)，和普通极坐标一致。 */
static calc_complex_t setting_origin;

/* Exported functions --------------------------------------------------------*/

void settings_init(void)
{
  setting_value[SETTING_ANGLE] = 0U;      /* DEG */
  setting_value[SETTING_COMPLEX] = 1U;    /* CMPLX：开机就允许复数 */
  setting_value[SETTING_POLAR] = 0U;      /* RECT */

  setting_pending[SETTING_ANGLE] = setting_value[SETTING_ANGLE];
  setting_pending[SETTING_COMPLEX] = setting_value[SETTING_COMPLEX];
  setting_pending[SETTING_POLAR] = setting_value[SETTING_POLAR];

  setting_origin.real = 0.0f;
  setting_origin.imag = 0.0f;
}

uint8_t settings_value(setting_id_t id)
{
  return setting_value[id];
}

uint8_t settings_pending(setting_id_t id)
{
  return setting_pending[id];
}

void settings_begin(setting_id_t id)
{
  setting_pending[id] = setting_value[id];     /* 暂存同步成生效值 */
}

void settings_select(setting_id_t id, uint8_t value)
{
  setting_pending[id] = (uint8_t)((value != 0U) ? 1U : 0U);
}

uint8_t settings_commit(setting_id_t id)
{
  if (setting_pending[id] == setting_value[id])
  {
    return 0U;                                 /* 没改过，不用重算 */
  }

  setting_value[id] = setting_pending[id];
  return 1U;
}

void settings_discard(setting_id_t id)
{
  setting_pending[id] = setting_value[id];
}

void settings_set_value(setting_id_t id, uint8_t value)
{
  setting_value[id] = (uint8_t)((value != 0U) ? 1U : 0U);
  setting_pending[id] = setting_value[id];     /* 快捷操作：暂存一起同步 */
}

calc_complex_t settings_origin(void)
{
  return setting_origin;
}

void settings_set_origin(calc_complex_t origin)
{
  setting_origin = origin;
}
