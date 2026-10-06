#ifndef CALC_SERIAL_RX_H
#define CALC_SERIAL_RX_H

#include <stdint.h>

#include "calc_view.h"

/*
 * 串口接收内容的"行缓冲 + 显示规则"：
 *   1. 收到回车或换行 → 这一行留在屏幕上，下一批字符到来时再清行；
 *   2. "\r\n" 只算一次换行；
 *   3. 退格 → 删掉最后一个字符（补空格）；
 *   4. 一行满 16 格 → 整行左移一格，始终显示最新收到的内容。
 *
 * 纯逻辑：不 include HAL、不碰 USB，所以能在 PC 上单测。
 * 谁把字节喂进来（比如 USB 轮询）由页面决定。
 */

/* 上电初始化：清空行缓冲，并把 initial_text 作为占位内容（传 NULL 就是全空格）。 */
void serial_rx_init(const char *initial_text);

/* 收到一个字节（换行/退格/可见字符分别处理）。 */
void serial_rx_feed(uint8_t byte);

/* 当前要显示的那一行（固定 VIEW_LINE_WIDTH 格，右侧空格补齐）。 */
const char *serial_rx_text(void);

#endif
