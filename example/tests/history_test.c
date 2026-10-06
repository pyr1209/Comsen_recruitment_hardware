/*
 * 历史页：直接编译仓库里真实的 calc_page_history.c + 历史/输入/结果/设置/界面模块，
 * 验证搬家前后行为一致：空历史提示、翻条目、滚窗口、OK/EXE 的动作、退出路径。
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "calc_history.h"
#include "calc_input.h"
#include "calc_keymap.h"
#include "calc_page_history.h"
#include "calc_settings.h"
#include "calc_ui.h"

static unsigned failures;

static void expect_int(const char *label, int actual, int wanted)
{
  if (actual != wanted)
  {
    printf("FAIL %-34s got %d, want %d\n", label, actual, wanted);
    failures++;
  }
  else
  {
    printf("ok   %-34s %d\n", label, actual);
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
    printf("FAIL %-34s \"%s\" want \"%s\"\n", label, shown, wanted);
    failures++;
  }
  else
  {
    printf("ok   %-34s \"%s\"\n", label, shown);
  }
}

static calc_complex_t make(float real, float imag)
{
  calc_complex_t value;

  value.real = real;
  value.imag = imag;
  return value;
}

static void render(const calc_history_t *history, char top[VIEW_LINE_WIDTH],
                   char bottom[VIEW_LINE_WIDTH])
{
  memset(top, 0, VIEW_LINE_WIDTH);
  memset(bottom, 0, VIEW_LINE_WIDTH);
  history_page_render(history, top, bottom);
}

int main(void)
{
  calc_history_t history;
  calc_input_t input;
  char top[VIEW_LINE_WIDTH];
  char bottom[VIEW_LINE_WIDTH];

  /* 空历史：第 1 行 HISTORY、第 2 行 EMPTY，方向键不乱动。 */
  calc_history_clear(&history);
  history_page_reset();
  render(&history, top, bottom);
  expect_line("empty: top", top, "HISTORY         ");
  expect_line("empty: bottom", bottom, "EMPTY           ");
  expect_int("empty: UP 无动作",
             history_page_handle_key(TTP229_KEY_UP, &history), HISTORY_ACTION_NONE);
  expect_int("empty: OK 只回算式页",
             history_page_handle_key(TTP229_KEY_OK, &history), HISTORY_ACTION_NONE);
  expect_int("empty: OK 后页面", ui_screen(), SCREEN_EXPR);

  /* 两条记录：1+2=3、再一条 6*7=42。index 0 应是最新的 6*7。 */
  calc_history_clear(&history);
  calc_history_push(&history, "1+2", make(3.0f, 0.0f));
  calc_history_push(&history, "6*7", make(42.0f, 0.0f));
  history_page_reset();
  render(&history, top, bottom);
  expect_line("新记录: top", top, "6*7             ");
  expect_line("新记录: bottom + 位置", bottom, "=42          1/2");

  expect_int("UP -> 更旧", history_page_handle_key(TTP229_KEY_UP, &history),
             HISTORY_ACTION_NONE);
  render(&history, top, bottom);
  expect_line("旧记录: top", top, "1+2             ");
  expect_line("旧记录: bottom + 位置", bottom, "=3           2/2");

  expect_int("再 UP 到底不动",
             history_page_handle_key(TTP229_KEY_UP, &history), HISTORY_ACTION_NONE);
  render(&history, top, bottom);
  expect_line("到底后仍是旧记录", top, "1+2             ");

  expect_int("DOWN -> 更新", history_page_handle_key(TTP229_KEY_DOWN, &history),
             HISTORY_ACTION_NONE);
  render(&history, top, bottom);
  expect_line("DOWN 后回到最新", top, "6*7             ");

  /* OK：把选中算式装进输入行，返回 LOAD，并回到算式页。 */
  calc_input_clear(&input);
  (void)calc_input_insert(&input, '9');
  expect_int("OK 返回 LOAD",
             history_page_handle_key(TTP229_KEY_OK, &history), HISTORY_ACTION_LOAD);
  expect_int("OK 后页面", ui_screen(), SCREEN_EXPR);
  expect_int("load 成功", history_page_load(&history, &input), 1);
  expect_int("load 复写输入行长度", input.length, 3);
  expect_int("load 内容", memcmp(input.text, "6*7", 3), 0);

  /* EXE：同样装入，但由调用方决定继续重算。 */
  expect_int("EXE 返回 LOAD_EVAL",
             history_page_handle_key(TTP229_KEY_EXE, &history),
             HISTORY_ACTION_LOAD_EVAL);

  /* 长算式：窗口最多滑到末 16 格，再往右不动。 */
  calc_history_clear(&history);
  calc_history_push(&history,
                    "1234567890123456789012345", make(1.0f, 0.0f));
  history_page_reset();
  render(&history, top, bottom);
  expect_line("长算式: 窗口起点", top, "1234567890123456");
  for (uint8_t step = 0U; step < 40U; ++step)
  {
    (void)history_page_handle_key(TTP229_KEY_RIGHT, &history);
  }
  render(&history, top, bottom);
  expect_line("长算式: 右滚到底夹住", top, "0123456789012345");
  (void)history_page_handle_key(TTP229_KEY_LEFT, &history);
  render(&history, top, bottom);
  expect_line("长算式: 左滚一格", top, "9012345678901234");

  /* 退出路径。 */
  expect_int("BACK -> 菜单",
             history_page_handle_key(TTP229_KEY_BACK, &history), HISTORY_ACTION_NONE);
  expect_int("BACK 后页面", ui_screen(), SCREEN_MENU);
  expect_int("MODE -> 算式页",
             history_page_handle_key(TTP229_KEY_MODE, &history), HISTORY_ACTION_NONE);
  expect_int("MODE 后页面", ui_screen(), SCREEN_EXPR);

  if (failures != 0U)
  {
    printf("\n%d 项失败\n", failures);
    return 1;
  }
  printf("\nhistory page: 全部通过\n");
  return 0;
}
