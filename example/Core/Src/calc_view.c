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

/* 选项前面的标记：选中的写 '>'，没被选但正生效的写 '*'。 */
static char option_marker(uint8_t pending, uint8_t active, uint8_t option)
{
  if (pending == option)
  {
    return '>';
  }
  if (active == option)
  {
    return '*';
  }

  return ' ';
}

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

void view_option_line(char line[VIEW_LINE_WIDTH],
                      const char *first, uint8_t first_position,
                      const char *second, uint8_t second_position,
                      uint8_t pending, uint8_t active)
{
  uint8_t index;

  /* 先整行清空：两个选项之间的格子也要是空格，不能留下上一屏的字符。 */
  view_fill(line, ' ');

  /* 标记放在名字前面一格，名字本身接着写。 */
  line[first_position - 1U] = option_marker(pending, active, 0U);
  for (index = 0U; (first[index] != '\0') &&
                   ((uint8_t)(first_position + index) < VIEW_LINE_WIDTH); ++index)
  {
    line[first_position + index] = first[index];
  }

  line[second_position - 1U] = option_marker(pending, active, 1U);
  for (index = 0U; (second[index] != '\0') &&
                   ((uint8_t)(second_position + index) < VIEW_LINE_WIDTH); ++index)
  {
    line[second_position + index] = second[index];
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
