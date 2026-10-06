/**
  ******************************************************************************
  * @file    calc_view.c
  * @brief   显示层的行工具：把数据拼成 1602 的一行。
  *
  *          这三个函数原来在 main.c 里，是无状态的（只碰传进来的缓冲，
  *          不读不写全局、不碰 HAL），所以单独搬到这里：既能被所有页面共用，
  *          又能在 PC 上单测。窗口起点这种"显示记账"由调用方持有并传指针进来。
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "calc_view.h"

/* Exported functions --------------------------------------------------------*/

void view_fill(char line[VIEW_LINE_WIDTH], char character)
{
  uint8_t index;

  for (index = 0U; index < VIEW_LINE_WIDTH; ++index)
  {
    line[index] = character;
  }
}

void view_set_text(char line[VIEW_LINE_WIDTH], const char *text)
{
  uint8_t index = 0U;

  view_fill(line, ' ');
  while ((index < VIEW_LINE_WIDTH) && (text[index] != '\0'))
  {
    line[index] = text[index];
    index++;
  }
}

void view_format_input(const calc_input_t *input, uint8_t *window,
                       char line[VIEW_LINE_WIDTH], uint8_t *cursor_column)
{
  uint8_t column;

  /* 光标跑到窗口左边 → 窗口跟着左移；跑到右边外面 → 窗口右移。 */
  if (input->cursor < *window)
  {
    *window = input->cursor;
  }
  else if (input->cursor >= (uint8_t)(*window + VIEW_LINE_WIDTH))
  {
    *window = (uint8_t)(input->cursor - (VIEW_LINE_WIDTH - 1U));
  }

  for (column = 0U; column < VIEW_LINE_WIDTH; ++column)
  {
    const uint8_t index = (uint8_t)(*window + column);

    /* 算式以外的格子留空格，末尾光标就落在空格上。 */
    line[column] = (index < input->length) ? input->text[index] : ' ';
  }

  *cursor_column = (uint8_t)(input->cursor - *window);
}
