#ifndef CALC_PAGE_SERIAL_H
#define CALC_PAGE_SERIAL_H

#include <stdint.h>

#include "calc_view.h"

/*
 * "SEND FROM PC" 页面：把电脑通过 USB 虚拟串口发来的内容显示出来。
 *
 * 这个页面自己管自己的状态（显示行、长度、是否刚开新行、\r\n 去重），
 * 别人只需要按节奏调 serial_page_poll()，按键和绘制各一个入口。
 */

/* 上电初始化：显示行的初始内容。 */
void serial_page_init(void);

/* 每 20 ms 调一次：从 USB 环形缓冲取出收到的字节并更新显示行。 */
void serial_page_poll(void);

/* 按键：BACK 回菜单，MODE 回算式界面，其余忽略。 */
void serial_page_handle_key(uint8_t key);

/* 画这个页面的两行：第 1 行标题，第 2 行收到的内容。 */
void serial_page_render(char top[VIEW_LINE_WIDTH], char bottom[VIEW_LINE_WIDTH]);

#endif
