/**
  ******************************************************************************
  * @file    calc_page_game.c
  * @brief   小游戏界面：小恐龙跳仙人掌。
  *
  *          原来这一页的代码散在 main.c 里（handle_game_key + format_game_over_line
  *          + 3 个 static 状态 + 主循环里那段 70 ms 的 tick）。现在整页搬到这里：
  *          游戏实例、节拍、按键映射、画面全归它管，主循环只要反复调 game_page_poll()。
  *
  *          规则本身在 dino_game.c（可 PC 单测）；本文件是接线，要 HAL_GetTick 和
  *          CGRAM 写入，所以只能上机验证。
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "calc_page_game.h"

#include "calc_keymap.h"
#include "calc_ui.h"
#include "dino_game.h"
#include "lcd_cgram.h"
#include "main.h"

/* Private variables ---------------------------------------------------------*/
static dino_game_t dino_game;        /* 小游戏状态 */
static uint32_t    next_game_tick;   /* 下一个游戏逻辑步的时刻（ms） */
static uint8_t     game_sky_night;   /* CGRAM 天空那格现在是白天还是黑夜（0xFF = 未知） */

/* Private functions ---------------------------------------------------------*/

static void format_game_over_line(uint16_t score, char line[VIEW_LINE_WIDTH])
{
  static const char prefix[] = "SCORE ";
  static const char suffix[] = " RETRY";
  char digits[4];
  uint8_t digit_count = 0U;
  uint8_t position = 0U;
  uint8_t index;

  if (score > 9999U)
  {
    score = 9999U;
  }

  do
  {
    digits[digit_count] = (char)('0' + (char)(score % 10U));
    digit_count++;
    score = (uint16_t)(score / 10U);
  } while ((score > 0U) && (digit_count < 4U));

  view_fill(line, ' ');

  for (index = 0U; (prefix[index] != '\0') && (position < VIEW_LINE_WIDTH); ++index)
  {
    line[position] = prefix[index];
    position++;
  }
  while ((digit_count > 0U) && (position < VIEW_LINE_WIDTH))
  {
    digit_count--;
    line[position] = digits[digit_count];
    position++;
  }
  for (index = 0U; (suffix[index] != '\0') && (position < VIEW_LINE_WIDTH); ++index)
  {
    line[position] = suffix[index];
    position++;
  }
}

void game_page_handle_key(uint8_t key)
{
  if (dino_game_is_over(&dino_game) != 0U)
  {
    if (key == TTP229_KEY_OK)
    {
      dino_game_reset(&dino_game);         /* 只有 OK 能重开 */
      next_game_tick = HAL_GetTick();
      game_sky_night = 0xFFU;              /* 重开回到白天，下一个逻辑步刷新天色 */
    }
    else if ((key == TTP229_KEY_MODE) || (key == TTP229_KEY_BACK) ||
             (key == TTP229_KEY_AC))
    {
      ui_switch_to(SCREEN_EXPR);
    }
    return;
  }

  switch (key)
  {
    case TTP229_KEY_UP:
    case TTP229_KEY_RIGHT:
      dino_game_jump(&dino_game);
      break;

    case TTP229_KEY_MODE:
    case TTP229_KEY_BACK:
    case TTP229_KEY_AC:
      ui_switch_to(SCREEN_EXPR);
      break;

    default:
      break;
  }
}

/* Exported functions --------------------------------------------------------*/

void game_page_init(void)
{
  dino_game_reset(&dino_game);
  next_game_tick = HAL_GetTick();
  game_sky_night = 0xFFU;             /* 让第一个逻辑步把天色点阵刷成白天 */
}

void game_page_enter(void)
{
  game_page_init();                   /* 每次进来都是新的一局 */
  ui_switch_to(SCREEN_GAME);
}

void game_page_poll(void)
{
  /* 只在游戏界面里推进；没到 70 ms 就什么都不做。 */
  if (ui_screen() != SCREEN_GAME)
  {
    return;
  }
  if ((int32_t)(HAL_GetTick() - next_game_tick) < 0)
  {
    return;
  }

  {
    const uint8_t night = dino_game_is_night(&dino_game);

    next_game_tick += DINO_GAME_TICK_MS;
    dino_game_tick(&dino_game);

    /* 白天黑夜每 10 秒交替：天色一变就把 CGRAM 那一格的点阵换掉
       （云 ↔ 星星），屏幕上飘的东西自动跟着变。 */
    if (night != game_sky_night)
    {
      game_sky_night = night;
      lcd_cgram_define_sky(night);
    }
  }
}

void game_page_render(char top[VIEW_LINE_WIDTH], char bottom[VIEW_LINE_WIDTH])
{
  if (dino_game_is_over(&dino_game) != 0U)
  {
    /* 撞了之后：第 1 行 GAME OVER，第 2 行给分数和重开提示 */
    view_set_text(top, "GAME OVER");
    format_game_over_line(dino_game.score, bottom);
  }
  else
  {
    /* 两行都是游戏画面：上面是云和腾空的恐龙，下面是地面、仙人掌和恐龙 */
    dino_game_render(&dino_game, top, bottom);
  }
}
