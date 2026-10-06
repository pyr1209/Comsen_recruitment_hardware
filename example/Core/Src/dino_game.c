/**
  ******************************************************************************
  * @file    dino_game.c
  * @brief   小恐龙跳仙人掌（2 行 × 16 格的横版跑酷）。
  *
  *          世界是一条无限长的地面线，物体都有"世界列坐标"，每走一步整体
  *          左移一格；恐龙固定站在第 DINO_GAME_DINO_COLUMN 列。跳一次在空中
  *          停 DINO_GAME_JUMP_TICKS 步，这几步里画到第 1 行，仙人掌从它脚下
  *          走过去就安全了。
  *
  *          本文件不碰 LCD、不碰按键、不调 HAL，纯状态机 + 生成两行字符，
  *          所以能在 PC 上直接编译测试。
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "dino_game.h"

#include "lcd_cgram.h"

/* Private define ------------------------------------------------------------*/
/*
 * 两丛仙人掌之间"空地"的格数（随机范围，越小越难）。
 * 注意这里算的是空地：生成一丛之后再等 空地 + 丛宽 格才刷下一丛，
 * 这样 2 连、3 连也不会挤到一起。
 */
#define DINO_CACTUS_GAP_MIN 8U
#define DINO_CACTUS_GAP_MAX 12U

/* 一丛仙人掌最多几根（1~3 根随机，成组出现）。 */
#define DINO_CACTUS_CLUSTER_MAX 3U

/* 云之间的间隔（格）。 */
#define DINO_CLOUD_GAP_MIN  3U
#define DINO_CLOUD_GAP_MAX  7U

/* 开局先空出一段距离，别一上来就撞。 */
#define DINO_CACTUS_FIRST_GAP 9U

/* Private functions ---------------------------------------------------------*/

/* 线性同余随机数：够游戏用，也不用连 rand() 那一套进来。 */
static uint32_t dino_random_next(dino_game_t *game)
{
  game->random = (game->random * 1103515245UL) + 12345UL;
  return (game->random >> 16U);
}

static uint8_t dino_random_range(dino_game_t *game, uint8_t low, uint8_t high)
{
  const uint8_t span = (uint8_t)(high - low + 1U);

  return (uint8_t)(low + (uint8_t)(dino_random_next(game) % span));
}

/* 找一个空槽放新物体；满了就丢掉（同时在场上的物体本来就很少）。 */
static void dino_spawn(dino_game_t *game, uint8_t kind, int8_t column)
{
  uint8_t index;

  for (index = 0U; index < DINO_GAME_OBJECT_MAX; ++index)
  {
    if (game->object[index].kind == (uint8_t)DINO_OBJECT_NONE)
    {
      game->object[index].kind = kind;
      game->object[index].column = column;
      return;
    }
  }
}

/* Exported functions --------------------------------------------------------*/

void dino_game_reset(dino_game_t *game)
{
  uint8_t index;

  for (index = 0U; index < DINO_GAME_OBJECT_MAX; ++index)
  {
    game->object[index].kind = (uint8_t)DINO_OBJECT_NONE;
    game->object[index].column = 0;
  }

  game->airborne = 0U;
  game->jump_steps = 0U;
  game->animation = 0U;
  game->over = 0U;
  game->score = 0U;
  game->ticks = 0U;
  game->random = 1U;                       /* 固定种子：每次开局一样，方便测试 */
  game->speed = DINO_SPEED_START;          /* 起步是慢速档的一半 */
  game->scroll = 0U;
  game->ramp_ticks = DINO_SPEED_RAMP_TICKS;
  game->cactus_countdown = DINO_CACTUS_FIRST_GAP;
  game->cloud_countdown = 1U;
}

void dino_game_jump(dino_game_t *game)
{
  if ((game->over != 0U) || (game->airborne != 0U))
  {
    return;                                /* 空中再按无效，得落地才能再跳 */
  }

  game->airborne = 1U;
  game->jump_steps = DINO_GAME_JUMP_STEPS;
}

uint8_t dino_game_is_over(const dino_game_t *game)
{
  return game->over;
}

uint8_t dino_game_is_night(const dino_game_t *game)
{
  /* 每 DINO_GAME_PHASE_TICKS 拍换一次天：0~143 白天，143~286 黑夜，如此交替 */
  return (uint8_t)((game->ticks / DINO_GAME_PHASE_TICKS) & 1U);
}

void dino_game_tick(dino_game_t *game)
{
  uint8_t index;

  if (game->over != 0U)
  {
    return;                                /* 撞了就冻住画面 */
  }

  game->ticks++;                           /* 昼夜计时：不管世界动不动都在走 */

  /* 1. 提速：每 DINO_SPEED_RAMP_TICKS 个逻辑步加一档，到上限封顶 */
  if (game->ramp_ticks > 0U)
  {
    game->ramp_ticks--;
  }
  if ((game->ramp_ticks == 0U) && (game->speed < DINO_SPEED_MAX))
  {
    game->speed++;
    game->ramp_ticks = DINO_SPEED_RAMP_TICKS;
  }

  /* 2. 世界滚动：速度累加器攒够一格才走一步。
        低速时几步才滚一格，高速时每步滚一格，逻辑步长本身不变。 */
  game->scroll = (uint8_t)(game->scroll + game->speed);
  if (game->scroll < DINO_SPEED_SCALE)
  {
    return;                                /* 这一步世界没动：什么都不会变 */
  }
  game->scroll = (uint8_t)(game->scroll - DINO_SPEED_SCALE);

  /* 3. 整个世界往左滚一格 */
  for (index = 0U; index < DINO_GAME_OBJECT_MAX; ++index)
  {
    if (game->object[index].kind != (uint8_t)DINO_OBJECT_NONE)
    {
      game->object[index].column--;
      if (game->object[index].column < -1)
      {
        game->object[index].kind = (uint8_t)DINO_OBJECT_NONE;
      }
    }
  }

  /* 4. 撞了吗：仙人掌正好走到恐龙那一列，而恐龙还在地面上。
        高矮不影响判定——高仙人掌只是画得高，一样要跳过去。 */
  for (index = 0U; index < DINO_GAME_OBJECT_MAX; ++index)
  {
    if (((game->object[index].kind == (uint8_t)DINO_OBJECT_CACTUS) ||
         (game->object[index].kind == (uint8_t)DINO_OBJECT_CACTUS_TALL)) &&
        (game->object[index].column == (int8_t)DINO_GAME_DINO_COLUMN) &&
        (game->airborne == 0U))
    {
      game->over = 1U;
      return;
    }
  }

  /* 5. 记分：仙人掌跨过恐龙那一列（从它身后走过去）就 +1。
        每根仙人掌只会经过第 1 列一次，所以不会重复计分。 */
  for (index = 0U; index < DINO_GAME_OBJECT_MAX; ++index)
  {
    if (((game->object[index].kind == (uint8_t)DINO_OBJECT_CACTUS) ||
         (game->object[index].kind == (uint8_t)DINO_OBJECT_CACTUS_TALL)) &&
        (game->object[index].column == (int8_t)(DINO_GAME_DINO_COLUMN - 1U)))
    {
      game->score++;
    }
  }

  /* 6. 跑动动画换帧（跟滚动同步，腿才不会滑） */
  game->animation ^= 1U;

  /* 7. 刷新的仙人掌：一次刷 1~3 根成组出现，高矮随机；
        下一丛的等待格数要把这一丛的宽度算进去，保证中间留出空地。 */
  if (game->cactus_countdown > 0U)
  {
    game->cactus_countdown--;
    if (game->cactus_countdown == 0U)
    {
      const uint8_t size = dino_random_range(game, 1U, DINO_CACTUS_CLUSTER_MAX);
      const uint8_t tall = dino_random_range(game, 0U, 1U);
      uint8_t placed;

      for (placed = 0U; placed < size; ++placed)
      {
        dino_spawn(game,
                   (tall != 0U) ? (uint8_t)DINO_OBJECT_CACTUS_TALL
                                : (uint8_t)DINO_OBJECT_CACTUS,
                   (int8_t)(DINO_GAME_COLUMNS - 1U + placed));
      }

      game->cactus_countdown = (uint8_t)(dino_random_range(game, DINO_CACTUS_GAP_MIN,
                                                           DINO_CACTUS_GAP_MAX) +
                                          (uint8_t)(size - 1U));
    }
  }

  /* 8. 刷新的云/星星（纯背景，不参与碰撞；点阵由 lcd_cgram_define_sky 切换） */
  if (game->cloud_countdown > 0U)
  {
    game->cloud_countdown--;
    if (game->cloud_countdown == 0U)
    {
      dino_spawn(game, (uint8_t)DINO_OBJECT_SKY,
                 (int8_t)(DINO_GAME_COLUMNS - 1U));
      game->cloud_countdown = dino_random_range(game, DINO_CLOUD_GAP_MIN,
                                                DINO_CLOUD_GAP_MAX);
    }
  }

  /* 9. 空中计时按"格"算，放在最后：这一格滚完才减，所以碰撞判定都算"在空中" */
  if (game->airborne != 0U)
  {
    game->jump_steps--;
    if (game->jump_steps == 0U)
    {
      game->airborne = 0U;
    }
  }
}

void dino_game_render(const dino_game_t *game, char top[DINO_GAME_COLUMNS],
                      char bottom[DINO_GAME_COLUMNS])
{
  uint8_t column;
  uint8_t index;
  const uint8_t night = dino_game_is_night(game);

  /* 第 1 行先清空，第 2 行铺成地面 */
  for (column = 0U; column < DINO_GAME_COLUMNS; ++column)
  {
    top[column] = ' ';
    bottom[column] = DINO_GAME_GROUND_CHAR;
  }

  for (index = 0U; index < DINO_GAME_OBJECT_MAX; ++index)
  {
    const int8_t at = game->object[index].column;

    if ((at < 0) || (at >= (int8_t)DINO_GAME_COLUMNS))
    {
      continue;
    }

    if (game->object[index].kind == (uint8_t)DINO_OBJECT_CACTUS)
    {
      bottom[at] = (char)LCD1602_CHAR_CACTUS;
    }
    else if (game->object[index].kind == (uint8_t)DINO_OBJECT_CACTUS_TALL)
    {
      bottom[at] = (char)LCD1602_CHAR_CACTUS_TALL;
    }
    else if (game->object[index].kind == (uint8_t)DINO_OBJECT_SKY)
    {
      top[at] = (char)LCD1602_CHAR_SKY;    /* 云还是星星由 CGRAM 那格的点阵决定 */
    }
  }

  /* 天黑以后第 1 行最左边挂一个月亮（画在背景之后，不会被云/星星盖住） */
  if (night != 0U)
  {
    top[0] = (char)LCD1602_CHAR_MOON;
  }

  /* 恐龙：在空中就画到第 1 行，否则画在地面那一行（跑动两帧交替） */
  if (game->airborne != 0U)
  {
    top[DINO_GAME_DINO_COLUMN] = (char)LCD1602_CHAR_DINO_JUMP;
  }
  else
  {
    bottom[DINO_GAME_DINO_COLUMN] = (char)((game->animation != 0U)
                                               ? LCD1602_CHAR_DINO_RUN2
                                               : LCD1602_CHAR_DINO_RUN1);
  }

  /* 分数：右对齐放在第 1 行最右边三格（最多三位，超过就停在 999）。
     最后画，所以不会被飘过去的云/星星盖住。 */
  {
    uint16_t value = (game->score > 999U) ? 999U : game->score;
    uint8_t digits = 1U;
    const uint8_t position = (uint8_t)(DINO_GAME_COLUMNS - DINO_GAME_SCORE_WIDTH);

    if (value >= 100U)
    {
      digits = 3U;
    }
    else if (value >= 10U)
    {
      digits = 2U;
    }

    for (index = 0U; index < digits; ++index)
    {
      /* 右对齐：个位总在最右边那一格，位数多了往左长 */
      top[(uint8_t)(position + DINO_GAME_SCORE_WIDTH - 1U - index)] =
          (char)('0' + (char)(value % 10U));
      value /= 10U;
    }
  }
}
