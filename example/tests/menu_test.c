/*
 * 菜单页 + 三个设置页：直接编译仓库里真实的 calc_page_menu.c + calc_settings.c +
 * calc_view.c，验证"按键 → 高亮项 / 暂存值 / 返回的动作"以及两行文本。
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "calc_keymap.h"
#include "calc_page_menu.h"
#include "calc_settings.h"
#include "calc_view.h"

static unsigned failures;

static void expect_int(const char *label, int actual, int wanted)
{
  if (actual != wanted)
  {
    printf("FAIL %-42s got %d, want %d\n", label, actual, wanted);
    failures++;
  }
  else
  {
    printf("ok   %-42s %d\n", label, actual);
  }
}

static void expect_line(const char *label, const char got[VIEW_LINE_WIDTH],
                        const char *wanted)
{
  char shown[VIEW_LINE_WIDTH + 1];

  memcpy(shown, got, VIEW_LINE_WIDTH);
  shown[VIEW_LINE_WIDTH] = '\0';

  if (strcmp(shown, wanted) != 0)
  {
    printf("FAIL %-42s \"%s\" want \"%s\"\n", label, shown, wanted);
    failures++;
  }
  else
  {
    printf("ok   %-42s \"%s\"\n", label, shown);
  }
}

static void show_menu(const char *label)
{
  char top[VIEW_LINE_WIDTH];
  char bottom[VIEW_LINE_WIDTH];

  menu_page_render(top, bottom);
  printf("     %-40s | %s |%s|\n", label, top, bottom);
}

int main(void)
{
  char top[VIEW_LINE_WIDTH];
  char bottom[VIEW_LINE_WIDTH];

  /* ---- 菜单页 ---- */
  menu_page_init();
  menu_page_render(top, bottom);
  expect_line("菜单: 第 1 行", top, "SELECT MODE     ");
  expect_line("菜单: 首项高亮", bottom, "> ANGLE UNIT    ");

  expect_int("DOWN 换到第 2 项", menu_page_handle_key(TTP229_KEY_DOWN),
             MENU_ACTION_NONE);
  menu_page_render(top, bottom);
  expect_line("第 2 项", bottom, "> COMPLEX       ");
  expect_int("RIGHT 也往下", menu_page_handle_key(TTP229_KEY_RIGHT),
             MENU_ACTION_NONE);
  menu_page_render(top, bottom);
  expect_line("第 3 项", bottom, "> POLAR         ");

  /* 第 3 项按 OK = 选 POLAR。 */
  expect_int("OK 选中 POLAR", menu_page_handle_key(TTP229_KEY_OK),
             MENU_ACTION_OPEN_POLAR);

  (void)menu_page_handle_key(TTP229_KEY_DOWN);      /* -> HISTORY */
  expect_int("OK 选中 HISTORY", menu_page_handle_key(TTP229_KEY_OK),
             MENU_ACTION_OPEN_HISTORY);
  (void)menu_page_handle_key(TTP229_KEY_DOWN);      /* -> GAME */
  expect_int("OK 选中 GAME", menu_page_handle_key(TTP229_KEY_OK),
             MENU_ACTION_OPEN_GAME);
  (void)menu_page_handle_key(TTP229_KEY_DOWN);      /* -> SEND FROM PC */
  menu_page_render(top, bottom);
  expect_line("最后一项名字完整", bottom, "> SEND FROM PC  ");
  expect_int("OK 选中串口页", menu_page_handle_key(TTP229_KEY_OK),
             MENU_ACTION_OPEN_SERIAL);

  /* 到顶再往下绕回第一项；在首项按 UP 绕到最后一项。 */
  (void)menu_page_handle_key(TTP229_KEY_DOWN);      /* 绕回 ANGLE UNIT */
  menu_page_render(top, bottom);
  expect_line("从末项绕回首项", bottom, "> ANGLE UNIT    ");
  expect_int("OK 选中 ANGLE UNIT", menu_page_handle_key(TTP229_KEY_OK),
             MENU_ACTION_OPEN_ANGLE);
  (void)menu_page_handle_key(TTP229_KEY_UP);        /* 首项 UP 绕到末项 */
  menu_page_render(top, bottom);
  expect_line("首项 UP 绕到末项", bottom, "> SEND FROM PC  ");

  /* 再走一项看 COMPLEX 的 OK。 */
  (void)menu_page_handle_key(TTP229_KEY_DOWN);      /* 0 ANGLE */
  (void)menu_page_handle_key(TTP229_KEY_DOWN);      /* 1 COMPLEX */
  expect_int("OK 选中 COMPLEX", menu_page_handle_key(TTP229_KEY_OK),
             MENU_ACTION_OPEN_COMPLEX);

  expect_int("BACK 退出到算式页", menu_page_handle_key(TTP229_KEY_BACK),
             MENU_ACTION_EXIT_EXPR);
  expect_int("MODE 也退出", menu_page_handle_key(TTP229_KEY_MODE),
             MENU_ACTION_EXIT_EXPR);
  expect_int("无关键无动作", menu_page_handle_key(16U), MENU_ACTION_NONE);

  /* ---- 设置页 ---- */
  settings_init();                       /* DEG / CMPLX / RECT */
  settings_begin(SETTING_ANGLE);
  option_page_render(SETTING_ANGLE, top, bottom);
  expect_line("角度页: 第 1 行", top, "ANGLE UNIT      ");
  expect_line("角度页: DEG 正在选", bottom, ">DEG       RAD  ");

  expect_int("DOWN 只是改暂存", option_page_handle_key(TTP229_KEY_DOWN, SETTING_ANGLE),
             OPTION_ACTION_NONE);
  expect_int("暂存变成 RAD", settings_pending(SETTING_ANGLE), 1);
  expect_int("生效值还没变", settings_value(SETTING_ANGLE), 0);
  option_page_render(SETTING_ANGLE, top, bottom);
  expect_line("角度页: 选 RAD 且标出生效的 DEG", bottom, "*DEG      >RAD  ");

  expect_int("BACK 丢弃并回菜单",
             option_page_handle_key(TTP229_KEY_BACK, SETTING_ANGLE),
             OPTION_ACTION_EXIT_MENU);
  expect_int("BACK 后暂存恢复成生效值", settings_pending(SETTING_ANGLE), 0);
  expect_int("BACK 后生效值没变", settings_value(SETTING_ANGLE), 0);

  (void)option_page_handle_key(TTP229_KEY_DOWN, SETTING_ANGLE);
  expect_int("MODE 也丢弃并回算式页",
             option_page_handle_key(TTP229_KEY_MODE, SETTING_ANGLE),
             OPTION_ACTION_EXIT_EXPR);
  expect_int("MODE 后暂存恢复", settings_pending(SETTING_ANGLE), 0);

  /* OK：值变了才要求重算。 */
  (void)option_page_handle_key(TTP229_KEY_DOWN, SETTING_ANGLE);
  expect_int("OK 提交并要求重算",
             option_page_handle_key(TTP229_KEY_OK, SETTING_ANGLE),
             OPTION_ACTION_APPLY_EXIT);
  expect_int("提交后生效值变成 RAD", settings_value(SETTING_ANGLE), 1);
  option_page_render(SETTING_ANGLE, top, bottom);
  expect_line("生效后 RAD 前面是 '>'", bottom, " DEG      >RAD  ");

  /* 值没变时按 OK：只回算式页，不要求重算。 */
  settings_begin(SETTING_ANGLE);
  expect_int("值没变时 OK 不重算",
             option_page_handle_key(TTP229_KEY_OK, SETTING_ANGLE),
             OPTION_ACTION_EXIT_EXPR);

  /* 另外两个设置页的标题和选项。 */
  option_page_render(SETTING_COMPLEX, top, bottom);
  expect_line("复数页: 第 1 行", top, "COMPLEX         ");
  expect_line("复数页: 选项行", bottom, " COMP     >CMPLX");
  option_page_render(SETTING_POLAR, top, bottom);
  expect_line("结果形式页: 第 1 行", top, "POLAR           ");
  expect_line("结果形式页: 选项行", bottom, ">RECT      POLAR");

  show_menu("（菜单当前渲染）");

  if (failures != 0U)
  {
    printf("\n%d 项失败\n", failures);
    return 1;
  }
  printf("\nmenu page: 全部通过\n");
  return 0;
}
