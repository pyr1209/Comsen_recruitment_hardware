/*
 * 结果流程：直接编译仓库里真实的 calc_result.c + 引擎 + 格式化 + 输入 + 历史 + 设置 + view，
 * 把"算式 → 求值 → 结果行"整条链路端到端测掉（以前只能靠逻辑镜像）。
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "calc_history.h"
#include "calc_input.h"
#include "calc_result.h"
#include "calc_settings.h"

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

static void set_expression(calc_input_t *input, const char *text)
{
  uint8_t index = 0U;

  calc_input_clear(input);
  while ((text[index] != '\0') && (index < CALC_INPUT_MAX))
  {
    (void)calc_input_insert(input, text[index]);
    index++;
  }
}

int main(void)
{
  calc_input_t input;
  calc_history_t history;
  char line[16];
  uint8_t before;

  settings_init();
  calc_result_init();
  calc_history_clear(&history);

  puts("场景 1：基本求值（带四则和优先级）");
  set_expression(&input, "1+2*3");
  calc_result_evaluate(&input, &history, 1U, line);
  expect_line("1+2*3", line, "=7");
  expect_u("结果进了历史", calc_history_count(&history), 1U);
  expect_line("历史里那条的算式", calc_history_get(&history, 0U)->expression, "1+2*3");

  puts("场景 2：三类错误都写进结果行，且不记历史");
  before = calc_history_count(&history);
  set_expression(&input, "1/0");
  calc_result_evaluate(&input, &history, 1U, line);
  expect_line("除以 0", line, "Div0 ERROR");
  set_expression(&input, "s(30");
  calc_result_evaluate(&input, &history, 1U, line);
  expect_line("括号不配", line, "Syntax ERROR");
  set_expression(&input, "t(90)");
  calc_result_evaluate(&input, &history, 1U, line);
  expect_line("tan 极点", line, "Math ERROR");
  expect_u("出错不记历史", calc_history_count(&history), before);

  puts("场景 3：复数 + Ans（A 就是上一次结果）");
  set_expression(&input, "3+4*i");
  calc_result_evaluate(&input, &history, 1U, line);
  expect_line("复数结果", line, "=3+4i");
  set_expression(&input, "A*i");
  calc_result_evaluate(&input, &history, 1U, line);
  expect_line("A*i（Ans 参与运算）", line, "=-4+3i");

  puts("场景 4：切 POLAR 后重新应用（空算式 → 重画上一次结果）");
  settings_set_value(SETTING_POLAR, 1U);
  set_expression(&input, "");
  calc_result_reapply(&input, line);
  expect_line("上一次结果改写成极坐标", line, "=5\x01" "143.1301");
  settings_set_value(SETTING_POLAR, 0U);
  calc_result_reapply(&input, line);
  expect_line("切回直角坐标", line, "=-4+3i");

  puts("场景 5：有算式时 reapply 会重算（且不重复记历史）");
  set_expression(&input, "2^10");
  before = calc_history_count(&history);
  calc_result_reapply(&input, line);
  expect_line("重算 2^10", line, "=1024");
  expect_u("重算不记历史", calc_history_count(&history), before);

  puts("场景 7：单独格式化一个值（历史页显示条目用）");
  {
    calc_complex_t value = {0.0f, 2.0f};

    calc_result_format(value, line);
    expect_line("纯虚数", line, "=2i");
  }

  puts("场景 8：清掉 Ans 之后 show_answer 返回 0");
  calc_result_clear_answer();
  expect_u("没有结果可画", calc_result_show_answer(line), 0U);

  puts("场景 9：状态码 → 提示文字");
  expect_line("Div0", calc_result_status_text(CALC_DIV_ZERO), "Div0 ERROR");
  expect_line("Domain", calc_result_status_text(CALC_DOMAIN), "Math ERROR");
  expect_line("Syntax", calc_result_status_text(CALC_SYNTAX), "Syntax ERROR");

  printf("\n%s（失败 %u 处）\n", failures == 0U ? "PASS" : "FAIL", failures);
  return failures == 0U ? 0 : 1;
}
