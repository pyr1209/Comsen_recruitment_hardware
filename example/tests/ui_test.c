/*
 * 界面状态机（状态部分）：直接编译仓库里真实的 calc_ui.c（不依赖 HAL），
 * 断言"上电在算式界面、切页后读回、每个页面都能来回切"。
 */
#include <stdint.h>
#include <stdio.h>

#include "calc_ui.h"

static unsigned failures;

static void expect_screen(const char *label, ui_screen_t actual, ui_screen_t wanted)
{
  if (actual != wanted)
  {
    printf("  FAIL %-40s 实际=%d 期望=%d\n", label, (int)actual, (int)wanted);
    failures++;
  }
  else
  {
    printf("  ok   %-40s = %d\n", label, (int)actual);
  }
}

int main(void)
{
  static const ui_screen_t all[] =
  {
    SCREEN_EXPR, SCREEN_MENU, SCREEN_ANGLE, SCREEN_COMPLEX, SCREEN_POLAR,
    SCREEN_SERIAL, SCREEN_HISTORY, SCREEN_GAME
  };
  unsigned index;

  puts("场景 1：上电默认在算式界面");
  ui_init();
  expect_screen("ui_init 后是 SCREEN_EXPR", ui_screen(), SCREEN_EXPR);

  puts("场景 2：切页后读回来是同一个页面");
  ui_switch_to(SCREEN_MENU);
  expect_screen("切到菜单", ui_screen(), SCREEN_MENU);
  ui_switch_to(SCREEN_HISTORY);
  expect_screen("切到历史", ui_screen(), SCREEN_HISTORY);
  ui_switch_to(SCREEN_EXPR);
  expect_screen("切回算式", ui_screen(), SCREEN_EXPR);

  puts("场景 3：每个页面都能切进去再出来（来回切不会串页）");
  for (index = 0U; index < (unsigned)(sizeof(all) / sizeof(all[0])); ++index)
  {
    ui_switch_to(all[index]);
    if (ui_screen() != all[index])
    {
      printf("  FAIL 第 %u 个页面切不进去（读到 %d）\n", index, (int)ui_screen());
      failures++;
    }
    else
    {
      printf("  ok   页面 %d 切进切出正常\n", (int)all[index]);
    }
  }

  puts("场景 4：再次 ui_init 会回到算式界面");
  ui_init();
  expect_screen("ui_init 复位", ui_screen(), SCREEN_EXPR);

  printf("\n%s（失败 %u 处）\n", failures == 0U ? "PASS" : "FAIL", failures);
  return failures == 0U ? 0 : 1;
}
