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
/* 仙人掌间隔（格）的随机范围：越小越难。速度越快，同样的格数来得越快。 */
#define DINO_CACTUS_GAP_MIN 6U
#define DINO_CACTUS_GAP_MAX 10U

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
  game->jump_ticks = 0U;
  game->animation = 0U;
  game->over = 0U;
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
  game->jump_ticks = DINO_GAME_JUMP_TICKS;
}

uint8_t dino_game_is_over(const dino_game_t *game)
{
  return game->over;
}

void dino_game_tick(dino_game_t *game)
{
  uint8_t index;

  if (game->over != 0U)
  {
    return;                                /* 撞了就冻住画面 */
  }

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
    /* 这一步世界没动，但空中计时照走（所以跳跃的墙钟时间不随速度变） */
    if (game->airborne != 0U)
    {
      game->jump_ticks--;
      if (game->jump_ticks == 0U)
      {
        game->airborne = 0U;
      }
    }
    return;
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

  /* 4. 撞了吗：仙人掌正好走到恐龙那一列，而恐龙还在地面上 */
  for (index = 0U; index < DINO_GAME_OBJECT_MAX; ++index)
  {
    if ((game->object[index].kind == (uint8_t)DINO_OBJECT_CACTUS) &&
        (game->object[index].column == (int8_t)DINO_GAME_DINO_COLUMN) &&
        (game->airborne == 0U))
    {
      game->over = 1U;
      return;
    }
  }

  /* 5. 跑动动画换帧（跟滚动同步，腿才不会滑） */
  game->animation ^= 1U;

  /* 6. 刷新的仙人掌（间隔按"格"算，所以速度越快来得越密） */
  if (game->cactus_countdown > 0U)
  {
    game->cactus_countdown--;
    if (game->cactus_countdown == 0U)
    {
      dino_spawn(game, (uint8_t)DINO_OBJECT_CACTUS,
                 (int8_t)(DINO_GAME_COLUMNS - 1U));
      game->cactus_countdown = dino_random_range(game, DINO_CACTUS_GAP_MIN,
                                                 DINO_CACTUS_GAP_MAX);
    }
  }

  /* 7. 刷新的云（纯背景，不参与碰撞） */
  if (game->cloud_countdown > 0U)
  {
    game->cloud_countdown--;
    if (game->cloud_countdown == 0U)
    {
      dino_spawn(game, (uint8_t)DINO_OBJECT_CLOUD,
                 (int8_t)(DINO_GAME_COLUMNS - 1U));
      game->cloud_countdown = dino_random_range(game, DINO_CLOUD_GAP_MIN,
                                                DINO_CLOUD_GAP_MAX);
    }
  }

  /* 8. 空中计时放在最后：起跳后的这 4 个逻辑步里，碰撞判定都算"在空中" */
  if (game->airborne != 0U)
  {
    game->jump_ticks--;
    if (game->jump_ticks == 0U)
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
    else if (game->object[index].kind == (uint8_t)DINO_OBJECT_CLOUD)
    {
      top[at] = (char)LCD1602_CHAR_CLOUD;
    }
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
}
