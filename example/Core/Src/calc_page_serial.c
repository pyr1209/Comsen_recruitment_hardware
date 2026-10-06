/**
  ******************************************************************************
  * @file    calc_page_serial.c
  * @brief   "SEND FROM PC" 页面：显示电脑经 USB 虚拟串口发来的内容。
  *
  *          原来这一页的代码散在 main.c 里（handle_view_key / serial_handle_byte
  *          / serial_poll + 4 个 static 变量）。现在整页搬到这里，对外只留
  *          init / poll / handle_key / render 四个入口。
  *
  *          "行缓冲 + 显示规则"（换行、退格、满行左移、\r\n 去重）在
  *          calc_serial_rx.c 里（纯逻辑、可 PC 单测）；本文件只负责
  *          "从 USB 取字节 → 喂给它"，以及按键和画屏这两件接线的事。
  *
  *          依赖：USB 驱动（usb_rx_read，取收到的字节）、界面状态机（切页）、
  *          显示层行工具（画一行）。它不知道求值器、设置、历史的存在。
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "calc_page_serial.h"

#include "calc_keymap.h"
#include "calc_serial_rx.h"
#include "calc_ui.h"
#include "usbd_cdc_if.h"

#include <string.h>

/* Private define ------------------------------------------------------------*/
/* 一屏显示不下时，整行左移一格，始终显示最新收到的字符。 */
#define SERIAL_BANNER  "SEND FROM PC"

/* Exported functions --------------------------------------------------------*/

void serial_page_init(void)
{
  serial_rx_init(SERIAL_BANNER);   /* 开机先显示标题，等电脑发内容 */
}

void serial_page_poll(void)
{
  uint8_t chunk[32];
  uint16_t count;
  uint16_t index;

  count = usb_rx_read(chunk, (uint16_t)sizeof(chunk));
  for (index = 0U; index < count; ++index)
  {
    serial_rx_feed(chunk[index]);
  }
}

void serial_page_handle_key(uint8_t key)
{
  switch (key)
  {
    case TTP229_KEY_BACK:
      ui_switch_to(SCREEN_MENU);
      break;

    case TTP229_KEY_MODE:
      ui_switch_to(SCREEN_EXPR);
      break;

    default:
      break;
  }
}

void serial_page_render(char top[VIEW_LINE_WIDTH], char bottom[VIEW_LINE_WIDTH])
{
  view_set_text(top, SERIAL_BANNER);
  (void)memcpy(bottom, serial_rx_text(), VIEW_LINE_WIDTH);
}
