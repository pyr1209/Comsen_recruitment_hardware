/**
  ******************************************************************************
  * @file    calc_input.c
  * @brief   算式输入缓冲：插入、删除、光标移动。
  *
  *          所有操作都作用在"字符数组 + 长度 + 光标"这三个量上，
  *          全部是 O(长度) 的搬移，长度最长 64，开销可以忽略。
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include <stddef.h>

#include "calc_input.h"

/* Exported functions --------------------------------------------------------*/

void calc_input_clear(calc_input_t *input)
{
  uint8_t index;

  if (input == NULL)
  {
    return;
  }

  input->length = 0U;
  input->cursor = 0U;

  for (index = 0U; index < CALC_INPUT_MAX; ++index)
  {
    input->skip[index] = 0U;
  }
}

uint8_t calc_input_insert(calc_input_t *input, char character)
{
  uint8_t index;

  if ((input == NULL) || (character == '\0'))
  {
    return 0U;
  }
  if (input->length >= CALC_INPUT_MAX)
  {
    return 0U;
  }

  /* 光标及其右边的字符整体右移一格，腾出插入位置。 */
  for (index = input->length; index > input->cursor; --index)
  {
    input->text[index] = input->text[index - 1U];
    input->skip[index] = input->skip[index - 1U];
  }

  input->text[input->cursor] = character;
  input->skip[input->cursor] = 0U;      /* 用户输入的字符不跳过 */
  input->cursor++;
  input->length++;

  return 1U;
}

void calc_input_mark_skip(calc_input_t *input, uint8_t from, uint8_t count)
{
  uint8_t index;

  if (input == NULL)
  {
    return;
  }

  for (index = 0U; index < count; ++index)
  {
    const uint8_t position = (uint8_t)(from + index);

    if (position < input->length)
    {
      input->skip[position] = 1U;
    }
  }
}

uint8_t calc_input_backspace(calc_input_t *input)
{
  uint8_t index;

  if ((input == NULL) || (input->cursor == 0U))
  {
    return 0U;
  }

  /* 把光标左边那一格删掉：它右边的字符整体左移。 */
  for (index = (uint8_t)(input->cursor - 1U); index < input->length; ++index)
  {
    input->text[index] = input->text[index + 1U];
    input->skip[index] = input->skip[index + 1U];
  }

  input->cursor--;
  input->length--;

  return 1U;
}

uint8_t calc_input_delete(calc_input_t *input)
{
  uint8_t index;

  if ((input == NULL) || (input->cursor >= input->length))
  {
    return 0U;
  }

  /* 光标位置不动，把它右边的字符整体左移。 */
  for (index = input->cursor; index < (uint8_t)(input->length - 1U); ++index)
  {
    input->text[index] = input->text[index + 1U];
    input->skip[index] = input->skip[index + 1U];
  }
  input->skip[input->length - 1U] = 0U;

  input->length--;

  return 1U;
}

uint8_t calc_input_move_left(calc_input_t *input)
{
  if ((input == NULL) || (input->cursor == 0U))
  {
    return 0U;
  }

  input->cursor--;
  /* 跳过模板自带的固定字符 */
  while ((input->cursor > 0U) && (input->skip[input->cursor - 1U] != 0U))
  {
    input->cursor--;
  }
  return 1U;
}

uint8_t calc_input_move_right(calc_input_t *input)
{
  if ((input == NULL) || (input->cursor >= input->length))
  {
    return 0U;
  }

  input->cursor++;
  /* 跳过模板自带的固定字符 */
  while ((input->cursor < input->length) && (input->skip[input->cursor] != 0U))
  {
    input->cursor++;
  }
  return 1U;
}

uint8_t calc_input_move_home(calc_input_t *input)
{
  if ((input == NULL) || (input->cursor == 0U))
  {
    return 0U;
  }

  input->cursor = 0U;
  return 1U;
}

uint8_t calc_input_move_end(calc_input_t *input)
{
  if ((input == NULL) || (input->cursor == input->length))
  {
    return 0U;
  }

  input->cursor = input->length;
  return 1U;
}
