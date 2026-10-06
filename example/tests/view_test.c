/*
 * 显示层行工具：直接编译仓库里真实的 calc_view.c（它不依赖 HAL），
 * 用假数据断言"一行怎么拼、窗口怎么滚、光标落在第几列"。
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "calc_view.h"

static unsigned failures;

static void expect_line(const char *label, const char *actual, const char *wanted)
{
  char shown[17];
  int8_t end;

  memcpy(shown, actual, 16U);
  shown[16] = '\0';
  for (end = 15; end >= 0; --end)
  {
    if (shown[end] != ' ') break;
    shown[end] = '\0';
  }

  if (strcmp(shown, wanted) != 0)
  {
    printf("  FAIL %-40s 实际=\"%s\" 期望=\"%s\"\n", label, shown, wanted);
    failures++;
  }
  else
  {
    printf("  ok   %-40s = \"%s\"\n", label, shown);
  }
}

static void expect_u(const char *label, uint32_t actual, uint32_t wanted)
{
  if (actual != wanted)
  {
    printf("  FAIL %-40s 实际=%u 期望=%u\n", label, (unsigned)actual, (unsigned)wanted);
    failures++;
  }
  else
  {
    printf("  ok   %-40s = %u\n", label, (unsigned)actual);
  }
}

static void set_input(calc_input_t *in, const char *text, uint8_t cursor)
{
  uint8_t index = 0U;

  calc_input_clear(in);
  while ((text[index] != '\0') && (index < CALC_INPUT_MAX))
  {
    (void)calc_input_insert(in, text[index]);
    index++;
  }
  in->cursor = cursor;
}

int main(void)
{
  char line[16];
  calc_input_t input;
  uint8_t window;
  uint8_t cursor_column;
  uint8_t index;

  puts("场景 1：view_fill / view_set_text");
  view_fill(line, '-');
  expect_line("整行填 '-'", line, "----------------");

  view_set_text(line, "READY");
  expect_line("短字符串右边补空格", line, "READY");

  view_set_text(line, "1234567890123456");
  expect_line("正好 16 格", line, "1234567890123456");

  view_set_text(line, "12345678901234567890");
  expect_line("超长自动截断", line, "1234567890123456");

  view_set_text(line, "");
  {
    uint8_t spaces = 0U;

    for (index = 0U; index < 16U; ++index)
    {
      if (line[index] == ' ') spaces++;
    }
    expect_u("空串 → 整行空格", spaces, 16U);
  }

  puts("场景 2：view_format_input —— 短算式（窗口不用动）");
  set_input(&input, "1+2", 3U);
  window = 0U;
  view_format_input(&input, &window, line, &cursor_column);
  expect_line("显示算式", line, "1+2");
  expect_u("窗口仍在 0", window, 0U);
  expect_u("光标在第 3 列", cursor_column, 3U);

  puts("场景 3：长算式 —— 光标在末尾时窗口右滚");
  set_input(&input, "12345678901234567890", 20U);   /* 20 个字符 */
  window = 0U;
  view_format_input(&input, &window, line, &cursor_column);
  expect_u("窗口滚到 20-15=5", window, 5U);
  expect_line("显示最后 16 格", line, "678901234567890");   /* 从第 5 格开始 */
  expect_u("光标落在最后一列", cursor_column, 15U);

  puts("场景 4：长算式 —— 光标跑到窗口左边时窗口左滚");
  set_input(&input, "12345678901234567890", 3U);
  window = 5U;
  view_format_input(&input, &window, line, &cursor_column);
  expect_u("窗口跟到光标处", window, 3U);
  expect_u("光标落在第 0 列", cursor_column, 0U);

  puts("场景 5：长算式 —— 光标刚好越过窗口右边界");
  set_input(&input, "12345678901234567890", 16U);
  window = 0U;
  view_format_input(&input, &window, line, &cursor_column);
  expect_u("窗口 = 16-15", window, 1U);
  expect_u("光标落在最后一列", cursor_column, 15U);

  printf("\n%s（失败 %u 处）\n", failures == 0U ? "PASS" : "FAIL", failures);
  return failures == 0U ? 0 : 1;
}
