/*
 * 小游戏规则：直接编译仓库里真实的 dino_game.c（不碰硬件）。
 * 覆盖：起步速度、按格跳跃、高矮仙人掌、成组出现、计分、昼夜交替、画面。
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "dino_game.h"
#include "lcd_cgram.h"

#define LCD_COLUMNS 16U

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

static void expect_symbol(const char *label, char actual, char wanted)
{
  if (actual != wanted)
  {
    printf("  FAIL %-44s 实际=0x%02X 期望=0x%02X\n", label,
           (unsigned)(uint8_t)actual, (unsigned)(uint8_t)wanted);
    failures++;
  }
  else
  {
    printf("  ok   %-44s = 0x%02X\n", label, (unsigned)(uint8_t)actual);
  }
}

static void tick_n(dino_game_t *game, unsigned count)
{
  while (count-- > 0U)
  {
    dino_game_tick(game);
  }
}

static uint8_t is_cactus(uint8_t kind)
{
  return (uint8_t)((kind == (uint8_t)DINO_OBJECT_CACTUS) ||
                   (kind == (uint8_t)DINO_OBJECT_CACTUS_TALL));
}

/* 场上最靠左的仙人掌在第几列；没有就返回很小的数 */
static int nearest_cactus(const dino_game_t *game)
{
  int best = -1000;
  uint8_t index;

  for (index = 0U; index < DINO_GAME_OBJECT_MAX; ++index)
  {
    if (is_cactus(game->object[index].kind) != 0U)
    {
      const int column = (int)game->object[index].column;

      if ((best == -1000) || (column < best)) best = column;
    }
  }
  return best;
}

/* 同一行上最多连着几根仙人掌 */
static uint8_t widest_cluster(const dino_game_t *game)
{
  uint8_t best = 0U;
  uint8_t streak = 0U;
  int8_t column;

  for (column = 0; column < (int8_t)DINO_GAME_COLUMNS; ++column)
  {
    uint8_t index;
    uint8_t found = 0U;

    for (index = 0U; index < DINO_GAME_OBJECT_MAX; ++index)
    {
      if ((is_cactus(game->object[index].kind) != 0U) &&
          (game->object[index].column == column))
      {
        found = 1U;
      }
    }

    if (found != 0U)
    {
      streak++;
      if (streak > best) best = streak;
    }
    else
    {
      streak = 0U;
    }
  }
  return best;
}

static uint8_t count_char(const char line[DINO_GAME_COLUMNS], char wanted)
{
  uint8_t count = 0U;
  uint8_t column;

  for (column = 0U; column < DINO_GAME_COLUMNS; ++column)
  {
    if (line[column] == wanted) count++;
  }
  return count;
}

/* 清空场上的仙人掌，并让生成计时器暂时不要插手 */
static void clear_obstacles(dino_game_t *game)
{
  uint8_t index;

  for (index = 0U; index < DINO_GAME_OBJECT_MAX; ++index)
  {
    if (is_cactus(game->object[index].kind) != 0U)
    {
      game->object[index].kind = (uint8_t)DINO_OBJECT_NONE;
    }
  }
  game->cactus_countdown = 250U;
}

static void place_cacti(dino_game_t *game, int8_t first_column, uint8_t count,
                        uint8_t tall)
{
  uint8_t index;
  uint8_t placed = 0U;

  clear_obstacles(game);
  for (index = 0U; (index < DINO_GAME_OBJECT_MAX) && (placed < count); ++index)
  {
    if (game->object[index].kind == (uint8_t)DINO_OBJECT_NONE)
    {
      game->object[index].kind = (tall != 0U) ? (uint8_t)DINO_OBJECT_CACTUS_TALL
                                              : (uint8_t)DINO_OBJECT_CACTUS;
      game->object[index].column = (int8_t)(first_column + placed);
      placed++;
    }
  }
}

/* 会玩的机器人：最靠左的仙人掌走到第 4 列就起跳 */
static void robot_tick(dino_game_t *game)
{
  if ((dino_game_is_over(game) == 0U) && (nearest_cactus(game) == 4))
  {
    dino_game_jump(game);
  }
  dino_game_tick(game);
}

int main(void)
{
  dino_game_t game;
  char top[DINO_GAME_COLUMNS];
  char bottom[DINO_GAME_COLUMNS];

  puts("场景 1：开局");
  dino_game_reset(&game);
  expect_u("没结束", dino_game_is_over(&game), 0U);
  expect_u("分数 0", game.score, 0U);
  expect_u("起步速度 = 半速档", game.speed, DINO_SPEED_START);
  expect_u("开局是白天", dino_game_is_night(&game), 0U);
  dino_game_render(&game, top, bottom);
  expect_symbol("恐龙站在地面行", bottom[DINO_GAME_DINO_COLUMN],
                (char)LCD1602_CHAR_DINO_RUN1);
  expect_u("除恐龙外整行是地面", count_char(bottom, DINO_GAME_GROUND_CHAR),
           DINO_GAME_COLUMNS - 1U);
  expect_symbol("分数 0 显示在最右一格", top[LCD_COLUMNS - 1U], '0');

  puts("场景 2：起步速度是原来的一半（两拍滚一格）");
  {
    unsigned ticks_used = 0U;
    int before;

    while ((game.object[0].kind == (uint8_t)DINO_OBJECT_NONE) && (ticks_used < 20U))
    {
      dino_game_tick(&game);
      ticks_used++;
    }
    expect_u("第 1 个背景物体在第 2 拍进场", ticks_used, 2U);
    before = (int)game.object[0].column;
    dino_game_tick(&game);
    expect_u("下一拍世界不动", (uint32_t)(game.object[0].column == (int8_t)before), 1U);
    dino_game_tick(&game);
    expect_u("再一拍才左移一格",
             (uint32_t)(game.object[0].column == (int8_t)(before - 1)), 1U);
  }

  puts("场景 3：跳跃按格算（护住 6 格；半速下正好 12 拍）");
  dino_game_reset(&game);
  dino_game_jump(&game);
  expect_u("按一下跳到空中", game.airborne, 1U);
  expect_u("空中还剩 6 格", game.jump_steps, DINO_GAME_JUMP_STEPS);
  dino_game_jump(&game);
  expect_u("空中再按无效", game.jump_steps, DINO_GAME_JUMP_STEPS);
  tick_n(&game, 2U * (DINO_GAME_JUMP_STEPS - 1U));
  expect_u("滚完 5 格还在空中", game.airborne, 1U);
  tick_n(&game, 2U);
  expect_u("滚完 6 格落地", game.airborne, 0U);

  puts("场景 4：不跳就撞上，结束以后画面冻住");
  {
    char top_after[DINO_GAME_COLUMNS];
    char bottom_after[DINO_GAME_COLUMNS];

    dino_game_reset(&game);
    while ((dino_game_is_over(&game) == 0U) &&
           ((nearest_cactus(&game) < -1) || (nearest_cactus(&game) > 2)))
    {
      dino_game_tick(&game);
    }
    expect_u("仙人掌走到恐龙那一列就结束", dino_game_is_over(&game), 1U);
    dino_game_render(&game, top, bottom);
    tick_n(&game, 10U);
    dino_game_render(&game, top_after, bottom_after);
    expect_u("结束以后画面冻住",
             (uint32_t)((memcmp(top, top_after, DINO_GAME_COLUMNS) == 0) &&
                        (memcmp(bottom, bottom_after, DINO_GAME_COLUMNS) == 0)), 1U);
  }

  puts("场景 5：独立一根仙人掌越过 → +1 分");
  dino_game_reset(&game);
  place_cacti(&game, 3, 1U, 0U);
  dino_game_jump(&game);
  tick_n(&game, 8U);
  expect_u("没撞", dino_game_is_over(&game), 0U);
  expect_u("分数 +1", game.score, 1U);

  puts("场景 6：三连仙人掌（高矮混合）→ 跳过去 +3 分");
  dino_game_reset(&game);
  place_cacti(&game, 4, 3U, 1U);
  dino_game_render(&game, top, bottom);
  expect_symbol("高仙人掌画的是高精灵", bottom[4], (char)LCD1602_CHAR_CACTUS_TALL);
  expect_u("同一格不是矮精灵", (uint32_t)(bottom[4] != (char)LCD1602_CHAR_CACTUS), 1U);
  dino_game_jump(&game);
  tick_n(&game, 2U * DINO_GAME_JUMP_STEPS);
  expect_u("三连也能跳过去", dino_game_is_over(&game), 0U);
  expect_u("三根各记 1 分", game.score, 3U);

  puts("场景 7：矮仙人掌画另一种精灵");
  dino_game_reset(&game);
  place_cacti(&game, 5, 1U, 0U);
  dino_game_render(&game, top, bottom);
  expect_symbol("矮仙人掌", bottom[5], (char)LCD1602_CHAR_CACTUS);

  puts("场景 8：白天黑夜每 143 拍（10 秒）交替");
  dino_game_reset(&game);
  game.ticks = DINO_GAME_PHASE_TICKS - 1U;
  expect_u("第 142 拍还是白天", dino_game_is_night(&game), 0U);
  game.ticks = DINO_GAME_PHASE_TICKS;
  expect_u("第 143 拍进入黑夜", dino_game_is_night(&game), 1U);
  game.ticks = (uint16_t)(2U * DINO_GAME_PHASE_TICKS);
  expect_u("第 286 拍又回到白天", dino_game_is_night(&game), 0U);
  {
    char day_top[DINO_GAME_COLUMNS];

    game.ticks = 0U;
    game.object[0].kind = (uint8_t)DINO_OBJECT_SKY;
    game.object[0].column = 8;
    dino_game_render(&game, day_top, bottom);
    expect_symbol("天空装饰用同一格字符码", day_top[8], (char)LCD1602_CHAR_SKY);
    expect_symbol("白天第 1 行最左边没有月亮", day_top[0], ' ');
    game.ticks = DINO_GAME_PHASE_TICKS;
    dino_game_render(&game, top, bottom);
    expect_symbol("夜里出现月亮", top[0], (char)LCD1602_CHAR_MOON);
    expect_symbol("夜里的天空装饰还是同一格", top[8], (char)LCD1602_CHAR_SKY);
  }

  puts("场景 9：分数右对齐（最多三位）");
  dino_game_reset(&game);
  game.score = 7U;
  dino_game_render(&game, top, bottom);
  expect_symbol("7 → 最右一格", top[15], '7');
  game.score = 42U;
  dino_game_render(&game, top, bottom);
  expect_symbol("42 → 十位", top[14], '4');
  expect_symbol("42 → 个位", top[15], '2');
  game.score = 137U;
  dino_game_render(&game, top, bottom);
  expect_symbol("137 → 百位", top[13], '1');
  expect_symbol("137 → 十位", top[14], '3');
  expect_symbol("137 → 个位", top[15], '7');

  puts("场景 10：成组出现（机器人长跑，观察最大连排数）");
  {
    unsigned step;
    uint8_t widest = 0U;

    dino_game_reset(&game);
    for (step = 0U; step < 400U; ++step)
    {
      robot_tick(&game);
      if (widest_cluster(&game) > widest) widest = widest_cluster(&game);
      if (dino_game_is_over(&game) != 0U) break;
    }
    expect_u("400 拍没撞", dino_game_is_over(&game), 0U);
    expect_u("出现过 3 连", widest, 3U);
    expect_u("分数在涨", (uint32_t)(game.score > 0U), 1U);
    expect_u("这期间昼夜已经轮过",
             (uint32_t)(game.ticks >= (2U * DINO_GAME_PHASE_TICKS)), 1U);
  }

  puts("场景 11：同样的操作 → 同样的画面（固定种子）");
  {
    dino_game_t a;
    dino_game_t b;
    char top_b[DINO_GAME_COLUMNS];
    char bottom_b[DINO_GAME_COLUMNS];
    uint8_t same = 1U;
    unsigned step;

    dino_game_reset(&a);
    dino_game_reset(&b);
    for (step = 0U; step < 300U; ++step)
    {
      robot_tick(&a);
      robot_tick(&b);
      dino_game_render(&a, top, bottom);
      dino_game_render(&b, top_b, bottom_b);
      if ((memcmp(top, top_b, DINO_GAME_COLUMNS) != 0) ||
          (memcmp(bottom, bottom_b, DINO_GAME_COLUMNS) != 0))
      {
        same = 0U;
        break;
      }
    }
    expect_u("逐帧一致", same, 1U);
  }

  printf("\n%s（失败 %u 处）\n", failures == 0U ? "PASS" : "FAIL", failures);
  return failures == 0U ? 0 : 1;
}
