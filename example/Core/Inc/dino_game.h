#ifndef DINO_GAME_H
#define DINO_GAME_H

#include <stdint.h>

/*
 * 小恐龙跳仙人掌。屏幕是 2 行 × 16 格，两行都用来显示画面：
 *   第 1 行：飘过去的云，以及跳起来时出现在这里的恐龙
 *   第 2 行：地面（'_'）上的恐龙和仙人掌
 *
 * 逻辑和显示分开：本模块只算状态、只产出两行字符（用 CGRAM 精灵的字符码），
 * 不碰 LCD 也不碰按键，所以可以在 PC 上单独编译测试。
 */

#define DINO_GAME_COLUMNS      16U     /* 一行多少格 */
#define DINO_GAME_OBJECT_MAX   8U      /* 同时最多几个物体（仙人掌 + 云） */

#define DINO_GAME_DINO_COLUMN  2U      /* 恐龙固定站在第几列 */
#define DINO_GAME_TICK_MS      70U     /* 一个逻辑步多少毫秒 */
#define DINO_GAME_JUMP_TICKS   4U      /* 跳一次在空中待几个逻辑步 */

/*
 * 速度：单位是 1/DINO_SPEED_SCALE 格每逻辑步。
 *   开局 DINO_SPEED_START（= 0.5 格/步，也就是慢速档的一半路程），
 *   每 DINO_SPEED_RAMP_TICKS 个逻辑步加 1 档，爬到 DINO_SPEED_MAX 封顶
 *   （= 1 格/步，约 50 秒到顶）。
 * 逻辑步长（70 ms）不变，所以跳跃的墙钟时间始终是 4 步 ≈ 280 ms，手感不随速度变。
 */
#define DINO_SPEED_SCALE       16U
#define DINO_SPEED_START       8U
#define DINO_SPEED_MAX         16U
#define DINO_SPEED_RAMP_TICKS  90U

/* 地面那一行铺的字符（1602 里 '_' 正好画在字符格最下面一行）。 */
#define DINO_GAME_GROUND_CHAR  '_'

typedef enum
{
  DINO_OBJECT_NONE = 0,
  DINO_OBJECT_CACTUS,
  DINO_OBJECT_CLOUD
} dino_object_kind_t;

typedef struct
{
  int8_t  column;      /* 世界列坐标：每走一步减 1，走到 -1 就丢掉 */
  uint8_t kind;        /* dino_object_kind_t */
} dino_object_t;

typedef struct
{
  dino_object_t object[DINO_GAME_OBJECT_MAX];
  uint8_t       airborne;         /* 1 = 在空中（画到第 1 行） */
  uint8_t       jump_ticks;       /* 空中还剩几个逻辑步 */
  uint8_t       speed;            /* 当前速度，单位 1/DINO_SPEED_SCALE 格每步 */
  uint8_t       scroll;           /* 滚动累加器：攒够 DINO_SPEED_SCALE 走一格 */
  uint8_t       ramp_ticks;       /* 距离下一次提速还有几个逻辑步 */
  uint8_t       cactus_countdown; /* 还有几格刷下一个仙人掌 */
  uint8_t       cloud_countdown;  /* 还有几格刷下一朵云 */
  uint8_t       animation;        /* 跑动动画帧，每滚动一格翻转 */
  uint32_t      random;           /* 自己写的线性同余随机数状态 */
  uint8_t       over;             /* 1 = 撞上了 */
} dino_game_t;

void    dino_game_reset(dino_game_t *game);
void    dino_game_tick(dino_game_t *game);
void    dino_game_jump(dino_game_t *game);
uint8_t dino_game_is_over(const dino_game_t *game);

/* 把当前状态画成两行 16 格。返回的是字符码，不是可打印字符串。 */
void    dino_game_render(const dino_game_t *game, char top[DINO_GAME_COLUMNS],
                         char bottom[DINO_GAME_COLUMNS]);

#endif
