/*
 * 算式页面：直接编译仓库里真实的 calc_page_expr.c + 输入缓冲 + 显示层工具 + 设置，
 * 把"按键 → 缓冲内容 / 光标 / 返回的动作"整条链路测掉。
 * （搬家前这段只能靠人肉点键盘，现在能在这台机器上跑。）
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "calc_input.h"
#include "calc_keymap.h"
#include "calc_page_expr.h"
#include "calc_settings.h"
#include "calc_view.h"
#include "lcd1602.h"

/* 无 SHIFT 的字符键（键号照 calc_keymap.h / 键表）。 */
#define K_LPAREN 5U
#define K_RPAREN 6U
#define K_7      10U
#define K_8      11U
#define K_9      12U
#define K_4      15U
#define K_5      16U
#define K_6      17U
#define K_MUL    18U
#define K_DIV    19U
#define K_1      20U
#define K_2      21U
#define K_3      22U
#define K_PLUS   23U
#define K_MINUS  24U
#define K_0      25U
#define K_DOT    26U
#define K_EXP    27U

/* SHIFT 层。 */
#define S_PI     10U
#define S_ANGLE  11U
#define S_I      12U
#define S_E      15U
#define S_LOG    16U
#define S_LN     17U
#define S_POW    18U
#define S_SQRT   19U
#define S_SIN    20U
#define S_COS    21U
#define S_TAN    22U
#define S_ANS    28U

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

/* 比较算式内容（不含结束符）和光标位置。 */
static void expect_input(const char *label, const char *wanted, int wanted_cursor)
{
  const calc_input_t *input = expr_page_input();
  char shown[CALC_INPUT_MAX + 1];

  memcpy(shown, input->text, input->length);
  shown[input->length] = '\0';

  if ((strcmp(shown, wanted) != 0) || ((int)input->cursor != wanted_cursor))
  {
    printf("FAIL %-42s \"%s\"@%u want \"%s\"@%d\n",
           label, shown, input->cursor, wanted, wanted_cursor);
    failures++;
  }
  else
  {
    printf("ok   %-42s \"%s\"@%u\n", label, shown, input->cursor);
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

static expr_action_t press(uint8_t key)
{
  return expr_page_handle_key(key, 0U);      /* 不按 SHIFT 直接按 */
}

static expr_action_t press_shifted(uint8_t key)
{
  return expr_page_handle_key(key, 1U);      /* 按住 SHIFT 之后按 */
}

int main(void)
{
  char line[VIEW_LINE_WIDTH];
  uint8_t cursor;

  settings_init();

  /* 空算式：整行空格、光标在第 0 列。 */
  expr_page_init();
  expr_page_render(line, &cursor);
  expect_line("空算式: 第 1 行全空格", line, "                ");
  expect_int("空算式: 光标列", cursor, 0);

  /* 字符键照键表插入，光标跟着走。 */
  (void)press(K_4);
  (void)press(K_PLUS);
  (void)press(K_0);
  expect_input("4+0", "4+0", 3);
  (void)press(K_DOT);
  (void)press(K_0);
  expect_input("4+0.0", "4+0.0", 5);
  (void)press(K_7);
  (void)press(K_MUL);
  (void)press(K_8);
  expect_input("4+0.07*8", "4+0.07*8", 8);
  (void)press(K_EXP);                       /* 27 号键是 E（×10^x） */
  expect_input("追加 E", "4+0.07*8E", 9);

  /* 方向键：上/下 = 行首/行尾，左/右 = 一格。 */
  (void)press(TTP229_KEY_UP);
  expect_input("UP -> 行首", "4+0.07*8E", 0);
  (void)press(TTP229_KEY_DOWN);
  expect_input("DOWN -> 行尾", "4+0.07*8E", 9);
  (void)press(TTP229_KEY_LEFT);
  (void)press(TTP229_KEY_LEFT);
  expect_input("LEFT x2", "4+0.07*8E", 7);
  (void)press(TTP229_KEY_RIGHT);
  expect_input("RIGHT x1", "4+0.07*8E", 8);

  /* 在光标处插入：插进表达式中间，光标跟着走。 */
  (void)press(K_2);
  expect_input("中间插入 2", "4+0.07*82E", 9);

  /* DEL = 退格（删光标左边一格），BACK = 清整行。 */
  (void)press(TTP229_KEY_DEL);
  expect_input("DEL 退格", "4+0.07*8E", 8);
  (void)press(TTP229_KEY_BACK);
  expect_input("BACK 清整行", "", 0);

  /* 括号：光标右边已经是模板自带的 ')' 时只移过去，不插重复的。 */
  (void)press(K_LPAREN);
  (void)press(K_RPAREN);
  expect_input("普通左右括号", "()", 2);

  expr_page_init();
  (void)press_shifted(S_SIN);
  expect_input("shift 1 -> s()", "s()", 2);
  (void)press(K_RPAREN);
  expect_input("右括号不重复插", "s()", 3);
  (void)press(K_RPAREN);
  expect_input("光标已在末尾才真插", "s())", 4);

  /* AC：清空输入，并要求调用方把结果行拉回 READY。 */
  (void)press(K_4);
  expect_int("AC 返回 CLEAR_RESULT", press(TTP229_KEY_AC),
             EXPR_ACTION_CLEAR_RESULT);
  expect_input("AC 后输入为空", "", 0);

  /* MODE / EXE / OK：不自己切页，该交给调用方的就交出去。 */
  expect_int("MODE 返回 OPEN_MENU", press(TTP229_KEY_MODE), EXPR_ACTION_OPEN_MENU);
  expect_int("EXE 返回 EVALUATE", press(TTP229_KEY_EXE), EXPR_ACTION_EVALUATE);
  expect_int("OK 无动作", press(TTP229_KEY_OK), EXPR_ACTION_NONE);

  /* FMT：一键切换结果形式，并要求重画结果。 */
  settings_set_value(SETTING_POLAR, 0U);
  expect_int("FMT 返回 REFRESH_RESULT", press(TTP229_KEY_FMT),
             EXPR_ACTION_REFRESH_RESULT);
  expect_int("FMT 后切到极坐标", settings_value(SETTING_POLAR), 1);
  (void)press(TTP229_KEY_FMT);
  expect_int("再按 FMT 切回直角坐标", settings_value(SETTING_POLAR), 0);

  /* SHIFT 上档：常量。 */
  expr_page_init();
  (void)press_shifted(S_PI);
  expect_input("shift 7 -> pi", "pi", 2);
  (void)press_shifted(S_E);
  expect_input("shift 4 -> e", "pie", 3);
  (void)press_shifted(S_I);
  expect_input("shift 9 -> i", "piei", 4);
  (void)press_shifted(S_ANS);
  expect_input("shift FMT -> A（Ans）", "pieiA", 5);
  (void)press_shifted(S_ANGLE);
  expect_int("shift 8 -> ∠（CGRAM 字符码）", expr_page_input()->text[5],
             LCD1602_CHAR_ANGLE);

  /* SHIFT 上档：函数模板（自动补括号，光标落在括号里）。 */
  expr_page_init();
  (void)press_shifted(S_LN);
  expect_input("shift 8 -> ln()", "ln()", 3);
  expr_page_init();
  (void)press_shifted(S_SQRT);
  expect_input("shift / -> sqrt()", "sqrt()", 5);
  expr_page_init();
  (void)press_shifted(S_TAN);
  expect_input("shift 3 -> t()", "t()", 2);
  expr_page_init();
  (void)press_shifted(S_COS);
  expect_input("shift 2 -> c()", "c()", 2);

  /* log：模板 l(,)，光标停在第一个参数前面，填完按 → 去第二个参数。 */
  expr_page_init();
  (void)press_shifted(S_LOG);
  expect_input("shift 5 -> l(,)", "l(,)", 2);
  (void)press(K_2);
  expect_input("填第一个参数 2", "l(2,)", 3);
  (void)press(K_5);
  expect_input("填第一个参数 25", "l(25,)", 4);
  (void)press(TTP229_KEY_RIGHT);
  expect_input("→ 跳到第二个参数", "l(25,)", 5);
  (void)press(K_0);
  expect_input("填第二个参数 0", "l(25,0)", 6);

  /* x^y：没输底数 -> "()^()"；输好底数 -> "^()"，光标直接落进指数。 */
  expr_page_init();
  (void)press_shifted(S_POW);
  expect_input("shift 6 空算式 -> ()^()", "()^()", 1);
  (void)press(K_7);
  expect_input("把底数填进第一个括号", "(7)^()", 2);
  (void)press(TTP229_KEY_RIGHT);
  expect_input("→ 一次到指数位置", "(7)^()", 5);
  expr_page_init();
  (void)press(K_9);
  (void)press_shifted(S_POW);
  expect_input("shift 6 有底数 -> ^()", "9^()", 3);

  /* 长算式：只显示 16 格，光标始终在窗口里。 */
  expr_page_init();
  for (uint8_t digit = 0U; digit < 20U; ++digit)
  {
    (void)press(K_7);
  }
  expect_input("长算式: 内容", "77777777777777777777", 20);
  expr_page_render(line, &cursor);
  expect_int("长算式: 光标在屏内", cursor < VIEW_LINE_WIDTH, 1);
  /* 窗口滑到第 6..20 格：15 个 7，末尾那一格是光标所在的插入位。 */
  expect_line("长算式: 显示末 15 格 + 光标位", line, "777777777777777 ");
  expect_int("长算式: 光标在第 15 列", cursor, 15);

  if (failures != 0U)
  {
    printf("\n%d 项失败\n", failures);
    return 1;
  }
  printf("\nexpr page: 全部通过\n");
  return 0;
}
