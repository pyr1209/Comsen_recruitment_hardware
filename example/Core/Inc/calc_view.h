#ifndef CALC_VIEW_H
#define CALC_VIEW_H

#include <stdint.h>

#include "calc_input.h"

/* 1602 一行多少格。 */
#define VIEW_LINE_WIDTH 16U

/*
 * 显示层的"行工具"：把数据拼成 1602 的一行。
 *
 * 这几个函数只碰调用者传进来的缓冲，不读也不写任何全局变量，也不碰 HAL ——
 * 所以它们能在 PC 上单独编译、单独断言（见 tests 里的 view 测试）。
 */

/* 把整行填成同一个字符。 */
void view_fill(char line[VIEW_LINE_WIDTH], char character);

/* 把 C 字符串写进一行，右边补空格（超长自动截断）。 */
void view_set_text(char line[VIEW_LINE_WIDTH], const char *text);

/*
 * 画"两个选项"的那一行（设置页用）：每个选项前面一格放标记——
 * 选中的写 '>'，没被选但正生效的写 '*'，其余留空格。
 * 两个选项互斥，所以"选中且生效"只写 '>' 就够了。
 */
void view_option_line(char line[VIEW_LINE_WIDTH],
                      const char *first, uint8_t first_position,
                      const char *second, uint8_t second_position,
                      uint8_t pending, uint8_t active);

/*
 * 把算式渲染成一行，并给出光标所在列。
 *
 * 算式比屏幕宽时只显示一个"窗口"，窗口跟着光标走，保证光标始终可见：
 *   window 是窗口起点（显示窗口的记账，由调用方持有，比如算式界面自己的
 *   window_start；历史页用另外一份），本函数会在需要时把它往左/右挪。
 */
void view_format_input(const calc_input_t *input, uint8_t *window,
                       char line[VIEW_LINE_WIDTH], uint8_t *cursor_column);

#endif
