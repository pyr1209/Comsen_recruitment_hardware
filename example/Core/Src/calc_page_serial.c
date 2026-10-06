/**
  ******************************************************************************
  * @file    calc_page_serial.c
  * @brief   "SEND FROM PC" 页面：显示电脑经 USB 虚拟串口发来的内容。
  *
  *          原来这一页的代码散在 main.c 里（handle_view_key / serial_handle_byte
  *          / serial_poll + 4 个 static 变量）。现在整页搬到这里：自己的状态
  *          自己管，对外只留 init / poll / handle_key / render 四个入口。
  *
  *          依赖：USB 驱动（usb_rx_read，取收到的字节）、界面状态机（切页）、
  *          显示层行工具（画一行）。它不知道求值器、设置、历史的存在。
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "calc_page_serial.h"

#include "calc_keymap.h"
#include "calc_ui.h"
#include "usbd_cdc_if.h"

#include <string.h>

/* Private define ------------------------------------------------------------*/
/* 一屏显示不下时，整行左移一格，始终显示最新收到的字符。 */
#define SERIAL_BANNER  "SEND FROM PC"

/* Private variables ---------------------------------------------------------*/
static char    serial_line[VIEW_LINE_WIDTH];   /* 当前显示的这一行内容 */
static uint8_t serial_length;                  /* 已经放了几个字符 */
static uint8_t serial_started;                 /* 这一行是否已经开始（0 = 下次收到字符先清行） */
static uint8_t swallow_next_lf;                /* "\r\n" 只当一次换行 */

/* Private functions ---------------------------------------------------------*/

/* 收到一个字节：换行、退格、可见字符分别处理。 */
static void serial_handle_byte(uint8_t byte)
{
  /* 回车或换行：这一行显示完就保留在屏幕上，下次输入时再清掉。 */
  if ((byte == (uint8_t)'\r') || (byte == (uint8_t)'\n'))
  {
    if ((byte == (uint8_t)'\n') && (swallow_next_lf != 0U))
    {
      /* "\r\n" 只当作一次换行。 */
      swallow_next_lf = 0U;
      return;
    }
    swallow_next_lf = (byte == (uint8_t)'\r') ? 1U : 0U;
    serial_length = 0U;
    serial_started = 0U;
    return;
  }

  swallow_next_lf = 0U;

  if (byte == (uint8_t)'\b')
  {
    if (serial_length > 0U)
    {
      serial_length = (uint8_t)(serial_length - 1U);
      serial_line[serial_length] = ' ';
    }
    return;
  }

  if ((byte >= (uint8_t)0x20U) && (byte <= (uint8_t)0x7EU))
  {
    uint8_t index;

    /* 新的一行从清屏开始，避免上一行的残字混进来。 */
    if (serial_started == 0U)
    {
      view_fill(serial_line, ' ');
      serial_started = 1U;
    }

    if (serial_length < VIEW_LINE_WIDTH)
    {
      serial_line[serial_length] = (char)byte;
      serial_length = (uint8_t)(serial_length + 1U);
    }
    else
    {
      /* 这一行满了：整行左移一格。 */
      for (index = 0U; index < (VIEW_LINE_WIDTH - 1U); ++index)
      {
        serial_line[index] = serial_line[index + 1U];
      }
      serial_line[VIEW_LINE_WIDTH - 1U] = (char)byte;
    }
  }
}

/* Exported functions --------------------------------------------------------*/

void serial_page_init(void)
{
  serial_length = 0U;
  serial_started = 0U;
  swallow_next_lf = 0U;
  view_set_text(serial_line, SERIAL_BANNER);   /* 开机先显示标题，等电脑发内容 */
}

void serial_page_poll(void)
{
  uint8_t chunk[32];
  uint16_t count;
  uint16_t index;

  count = usb_rx_read(chunk, (uint16_t)sizeof(chunk));
  for (index = 0U; index < count; ++index)
  {
    serial_handle_byte(chunk[index]);
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
  (void)memcpy(bottom, serial_line, VIEW_LINE_WIDTH);
}
