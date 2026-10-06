#ifndef CALC_FORMAT_H
#define CALC_FORMAT_H

#include <stdint.h>

#include "calculator_engine.h"

/* 结果文本的宽度，正好是 1602 一行能显示的字符数。 */
#define CALC_FORMAT_WIDTH 16U

/*
 * 把计算结果格式化成 CALC_FORMAT_WIDTH 个字符（右侧空格补齐）：
 *   - 实数：普通写法，整数部分 + 最多 6 位小数，自动去掉尾随 0；
 *     数值过大或过小时用科学计数法；绝对值小于 5e-7 显示成 0。
 *   - 复数：polar = 0（RECT）写成 a+bi；polar = 1（POLAR）写成 r∠θ，
 *     其中 θ 按 angle_unit 决定显示成度还是弧度。实部/虚部为 0 时不写出来
 *     （例如 4i、而不是 0+4i）。
 *   - 非有限值：Math ERROR。
 * 不依赖 printf，也不需要链接浮点格式化代码。
 */
void calc_format_float(float value, char text[CALC_FORMAT_WIDTH]);

void calc_format_complex(calc_complex_t value, uint8_t polar,
                         calc_angle_unit_t angle_unit,
                         char text[CALC_FORMAT_WIDTH]);

#endif
