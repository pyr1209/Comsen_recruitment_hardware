#ifndef CALC_APP_H
#define CALC_APP_H

#include <stdint.h>

#include "calc_view.h"

/*
 * 应用层：把各个页面接成一台计算器。
 *
 * 它管三件事，正对应主循环里的三件事：
 *   app_init()          上电复位（界面、各页面、历史、Ans、上档锁存）
 *   app_handle_key()    按键分派：哪个键归哪个页面；页面返回的"动作"由这里落实
 *   app_render()        渲染分派：哪个页面负责画这两行、光标放哪
 * 另外还有两个周期性工作：app_poll_serial() / app_poll_game()。
 *
 * 它下面只依赖"页面 + 状态模块"，各页面不反向依赖它 —— 依赖方向保持单向，
 * 于是 main.c 只剩"芯片初始化 + 主循环节拍"，一行界面逻辑都没有。
 *
 * 这里包含页面（串口页要调 USB 驱动），所以本文件不参与 PC 单测；
 * 真正能单测的是它下面的那些模块。
 */

/* 上电：界面回算式页，各页面与共享状态（历史、Ans、SHIFT）复位。 */
void app_init(void);

/* 一个键号进来（0..29），由应用层分派给当前页面；
   SHIFT 的上档锁存也在这里，页面只管"上档之后这个键插什么"。 */
void app_handle_key(uint8_t key);

/* 每 20 ms 调一次：把 USB 收到的字节喂给串口页。 */
void app_poll_serial(void);

/* 每轮主循环调一次：小游戏页自己判断在不在游戏界面、到没到 70 ms。 */
void app_poll_game(void);

/* 生成当前页面的两行内容与光标位置（没有光标时写 CALC_VIEW_NO_CURSOR）。 */
void app_render(char top[VIEW_LINE_WIDTH], char bottom[VIEW_LINE_WIDTH],
                uint8_t *cursor_column, uint8_t *cursor_row);

#endif
