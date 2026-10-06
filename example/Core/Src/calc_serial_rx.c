/**
  ******************************************************************************
  * @file    calc_serial_rx.c
  * @brief   串口接收内容的行缓冲与显示规则（原来在 calc_page_serial.c 里）。
  *
  *          拆出来的理由：这四条规则（换行 / \r\n 去重 / 退格 / 满行左移）是
  *          真正需要反复试的"规则"，而它们不依赖 USB 和 HAL —— 放到这里之后
  *          就能在 PC 上单测，改规则不必再烧板子。页面那边只剩"取字节 → 喂进来"。
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include <stddef.h>

#include "calc_serial_rx.h"

/* Private variables ---------------------------------------------------------*/
static char    serial_line[VIEW_LINE_WIDTH];   /* 当前显示的这一行内容 */
static uint8_t serial_length;                  /* 已经放了几个字符 */
static uint8_t serial_started;                 /* 这一行是否已经开始（0 = 下次收到字符先清行） */
static uint8_t swallow_next_lf;                /* "\r\n" 只当一次换行 */

/* Exported functions --------------------------------------------------------*/

void serial_rx_init(const char *initial_text)
{
  serial_length = 0U;
  serial_started = 0U;
  swallow_next_lf = 0U;
  view_set_text(serial_line, (initial_text != NULL) ? initial_text : "");
}

void serial_rx_feed(uint8_t byte)
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

const char *serial_rx_text(void)
{
  return serial_line;
}
