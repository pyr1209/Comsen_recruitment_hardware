#ifndef CALC_PAGE_GAME_H
#define CALC_PAGE_GAME_H

#include <stdint.h>

#include "calc_view.h"

/*
 * 小游戏界面（小恐龙跳仙人掌）。
 *
 * 游戏规则在 dino_game.c（纯逻辑、可 PC 单测）；本页面负责两件接线的事：
 *   1. 管住游戏实例和"每 70 ms 推进一拍"的节拍（主循环只要反复调 game_page_poll）；
 *   2. 按键映射和画屏（含"GAME OVER / SCORE n RETRY"画面）。
 *
 * 这个文件要调 HAL_GetTick() 和 CGRAM 写入，所以不能在 PC 上编译——
 * 但它只是接线，真正的规则都在能测的 dino_game.c 里。
 */

/* 上电：建一局并把天色标记置为"待刷新"。 */
void game_page_init(void);

/* 从菜单进来：开新的一局，并切到游戏界面。 */
void game_page_enter(void);

/* 主循环每次循环都调一次：只在游戏界面里、且到 70 ms 才推进一拍。 */
void game_page_poll(void);

/* 游戏界面的按键：玩的时候 ↑ / → 跳、MODE/BACK/AC 退出；撞了以后只有 OK 重开。 */
void game_page_handle_key(uint8_t key);

/* 画游戏界面的两行（撞了以后换成结束画面）。 */
void game_page_render(char top[VIEW_LINE_WIDTH], char bottom[VIEW_LINE_WIDTH]);

#endif
