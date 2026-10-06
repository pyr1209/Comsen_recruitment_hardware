/*
 * 全局设置项：直接编译仓库里真实的 calc_settings.c（不依赖 HAL），
 * 断言"进页面同步 / 方向键只改暂存 / OK 提交 / BACK 丢弃 / 快捷改写 / 原点"。
 */
#include <stdint.h>
#include <stdio.h>

#include "calc_settings.h"

static unsigned failures;

static void expect_u(const char *label, uint32_t actual, uint32_t wanted)
{
  if (actual != wanted)
  {
    printf("  FAIL %-44s 实际=%u 期望=%u\n", label, (unsigned)actual, (unsigned)wanted);
    failures++;
  }
  else
  {
    printf("  ok   %-44s = %u\n", label, (unsigned)actual);
  }
}

int main(void)
{

  puts("场景 1：上电默认值");
  settings_init();
  expect_u("角度单位 = DEG", settings_value(SETTING_ANGLE), 0U);
  expect_u("数域 = CMPLX", settings_value(SETTING_COMPLEX), 1U);
  expect_u("结果形式 = RECT", settings_value(SETTING_POLAR), 0U);
  expect_u("暂存值同步（ANGLE）", settings_pending(SETTING_ANGLE), 0U);

  puts("场景 2：进设置页 → 暂存同步成生效值");
  settings_select(SETTING_ANGLE, 1U);          /* 上次留下的暂存（模拟脏状态） */
  settings_begin(SETTING_ANGLE);
  expect_u("begin 后暂存 = 生效", settings_pending(SETTING_ANGLE), 0U);

  puts("场景 3：方向键只改暂存，生效值不动");
  settings_select(SETTING_ANGLE, 1U);
  expect_u("暂存变 RAD", settings_pending(SETTING_ANGLE), 1U);
  expect_u("生效值仍是 DEG", settings_value(SETTING_ANGLE), 0U);

  puts("场景 4：OK 提交（返回 1 表示真的改了）");
  expect_u("提交返回 1", settings_commit(SETTING_ANGLE), 1U);
  expect_u("生效值 = RAD", settings_value(SETTING_ANGLE), 1U);
  expect_u("再提交一次返回 0（没改过，不需要重算）",
           settings_commit(SETTING_ANGLE), 0U);

  puts("场景 5：BACK / MODE 丢弃");
  settings_begin(SETTING_ANGLE);
  settings_select(SETTING_ANGLE, 0U);          /* 想改回 DEG */
  expect_u("暂存 = DEG", settings_pending(SETTING_ANGLE), 0U);
  settings_discard(SETTING_ANGLE);
  expect_u("丢弃后暂存恢复成生效值", settings_pending(SETTING_ANGLE), 1U);
  expect_u("生效值不受影响", settings_value(SETTING_ANGLE), 1U);

  puts("场景 6：快捷改写（FMT 用）—— 生效值和暂存值一起改");
  settings_set_value(SETTING_POLAR, 1U);
  expect_u("生效值 = POLAR", settings_value(SETTING_POLAR), 1U);
  expect_u("暂存值也同步", settings_pending(SETTING_POLAR), 1U);
  settings_set_value(SETTING_POLAR, 0U);
  expect_u("再切回 RECT", settings_value(SETTING_POLAR), 0U);

  puts("场景 7：三个设置项互不影响");
  settings_begin(SETTING_POLAR);
  settings_select(SETTING_POLAR, 1U);
  settings_commit(SETTING_POLAR);
  expect_u("POLAR = 1", settings_value(SETTING_POLAR), 1U);
  expect_u("ANGLE 没被动（仍 RAD）", settings_value(SETTING_ANGLE), 1U);
  expect_u("COMPLEX 没被动（仍 CMPLX）", settings_value(SETTING_COMPLEX), 1U);

  printf("\n%s（失败 %u 处）\n", failures == 0U ? "PASS" : "FAIL", failures);
  return failures == 0U ? 0 : 1;
}
