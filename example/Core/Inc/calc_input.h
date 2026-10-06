#ifndef CALC_INPUT_H
#define CALC_INPUT_H

#include <stdint.h>

/* 算式输入缓冲的最大长度（字符数，不含结束符）。 */
#define CALC_INPUT_MAX 64U

/*
 * 算式输入缓冲：内容、长度、光标位置三者配套。
 * 光标是"插入位置"，取值 0..length：0 表示在最前面，length 表示在末尾。
 * 本模块只负责数据，不碰屏幕和 GPIO。
 */
typedef struct
{
  char    text[CALC_INPUT_MAX];
  uint8_t length;
  uint8_t cursor;
  /* 模板字符标记：1 表示这一格是函数模板自带的固定字符（比如 l(,) 里的逗号），
     光标用方向键移动时自动跳过它们，这样能"从第一个参数直接跳到第二个参数"。 */
  uint8_t skip[CALC_INPUT_MAX];
} calc_input_t;

void    calc_input_clear(calc_input_t *input);
uint8_t calc_input_insert(calc_input_t *input, char character);
/* 把 text[from .. from+count-1] 标记/取消标记成"方向键跳过"的模板字符。 */
void    calc_input_mark_skip(calc_input_t *input, uint8_t from, uint8_t count);
uint8_t calc_input_backspace(calc_input_t *input);   /* 删光标左边一个字符 */
uint8_t calc_input_delete(calc_input_t *input);       /* 删光标所在位置的字符 */
uint8_t calc_input_move_left(calc_input_t *input);
uint8_t calc_input_move_right(calc_input_t *input);
uint8_t calc_input_move_home(calc_input_t *input);
uint8_t calc_input_move_end(calc_input_t *input);

#endif
