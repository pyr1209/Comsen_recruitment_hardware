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
#define DINO_GAME_OBJECT_MAX   12U     /* 同时最多几个物体（仙人掌 + 天空装饰） */

#define DINO_GAME_DINO_COLUMN  2U      /* 恐龙固定站在第几列 */
#define DINO_GAME_TICK_MS      70U     /* 一个逻辑步多少毫秒 */
/*
 * 跳一次在空中护住"多少格"（格 = 世界滚动的列数），不是拍数。
 * 按格算的好处是快慢都护一样远：高速档（1 格/拍）= 6 拍 ≈ 420 ms，
 * 低速档（0.5 格/拍）= 12 拍 ≈ 840 ms。连着的 3 根仙人掌占 3 格，
 * 6 格给它们留了 3 格余量，所以起跳时机很宽松。
 */
#define DINO_GAME_JUMP_STEPS   6U

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

/* 白天 / 黑夜每 10 秒换一次（10 秒 ÷ 70 ms ≈ 143 拍），来回交替。 */
#define DINO_GAME_PHASE_TICKS  143U

/* 地面那一行铺的字符（1602 里 '_' 正好画在字符格最下面一行）。 */
#define DINO_GAME_GROUND_CHAR  '_'

/* 分数显示在第 1 行最右边这几格里（最多三位，够一局用了）。 */
#define DINO_GAME_SCORE_WIDTH  3U

typedef enum
{
  DINO_OBJECT_NONE = 0,
  DINO_OBJECT_CACTUS,        /* 矮仙人掌 */
  DINO_OBJECT_CACTUS_TALL,   /* 高仙人掌（只是画得高，一样要跳过去） */
  DINO_OBJECT_SKY            /* 云 / 星星（白天黑夜共用一格点阵） */
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
  uint8_t       jump_steps;       /* 空中还剩几格（世界每滚一格减 1） */
  uint8_t       speed;            /* 当前速度，单位 1/DINO_SPEED_SCALE 格每步 */
  uint8_t       scroll;           /* 滚动累加器：攒够 DINO_SPEED_SCALE 走一格 */
  uint8_t       ramp_ticks;       /* 距离下一次提速还有几个逻辑步 */
  uint8_t       cactus_countdown; /* 还有几格刷下一个仙人掌 */
  uint8_t       cloud_countdown;  /* 还有几格刷下一朵云 */
  uint8_t       animation;        /* 跑动动画帧，每滚动一格翻转 */
  uint16_t      score;            /* 分数：每跨过一个仙人掌 +1 */
  uint16_t      ticks;            /* 开局到现在一共走了多少拍（用来切昼夜） */
  uint32_t      random;           /* 自己写的线性同余随机数状态 */
  uint8_t       over;             /* 1 = 撞上了 */
} dino_game_t;

void    dino_game_reset(dino_game_t *game);
void    dino_game_tick(dino_game_t *game);
void    dino_game_jump(dino_game_t *game);
uint8_t dino_game_is_over(const dino_game_t *game);
/* 1 = 已经天黑（15 秒以后）：云换成星星，第 1 行左边挂一个月亮 */
uint8_t dino_game_is_night(const dino_game_t *game);

/* 把当前状态画成两行 16 格。返回的是字符码，不是可打印字符串。 */
void    dino_game_render(const dino_game_t *game, char top[DINO_GAME_COLUMNS],
                         char bottom[DINO_GAME_COLUMNS]);

#endif
