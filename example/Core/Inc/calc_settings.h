#ifndef CALC_SETTINGS_H
#define CALC_SETTINGS_H

#include <stdint.h>

/*
 * 三个全局设置项：角度单位 / 数域 / 结果形式。
 *
 * 每个设置项都有两份值：
 *   value    生效值——求值和显示实际使用；
 *   pending  暂存值——只在设置页里被方向键改动。
 * 进设置页先"暂存 ← 生效"，方向键只改暂存；OK 提交（暂存 → 生效），
 * BACK / MODE 丢弃。这一套语义集中在本模块里，页面只管画和导航。
 *
 * 模块不读不写任何全局、不碰 HAL，也不认识"键号"——所以能在 PC 上单测。
 */

typedef enum
{
  SETTING_ANGLE = 0,     /* 0 = DEG, 1 = RAD */
  SETTING_COMPLEX,       /* 0 = COMP（只算实数）, 1 = CMPLX（允许复数） */
  SETTING_POLAR,         /* 0 = RECT（a+bi）, 1 = POLAR（r∠θ） */
  SETTING_COUNT
} setting_id_t;

/* 上电默认值：DEG / CMPLX / RECT。 */
void settings_init(void);

/* 生效值 / 暂存值（都是 0 或 1）。 */
uint8_t settings_value(setting_id_t id);
uint8_t settings_pending(setting_id_t id);

/* 进设置页：暂存 ← 生效。 */
void settings_begin(setting_id_t id);

/* 方向键：只改暂存值。 */
void settings_select(setting_id_t id, uint8_t value);

/* OK：暂存 → 生效；返回 1 表示值真的变了（调用方据此决定要不要重算）。 */
uint8_t settings_commit(setting_id_t id);

/* BACK / MODE：暂存 ← 生效（丢弃改动）。 */
void settings_discard(setting_id_t id);

/* 直接改写生效值（暂存一起同步）——给 FMT 一键切换这类快捷操作用。 */
void settings_set_value(setting_id_t id, uint8_t value);

#endif
