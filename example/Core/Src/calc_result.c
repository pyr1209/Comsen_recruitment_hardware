/**
  ******************************************************************************
  * @file    calc_result.c
  * @brief   结果流程：算式 → 求值 → 格式化 → 结果行，并记住上一次结果（Ans）。
  *
  *          这几段原来在 main.c 里（calc_evaluate_and_show / calc_reapply /
  *          calc_show_result / format_result_line / status_text + last_answer）。
  *          搬到这里之后：输入缓冲和历史由调用方传进来，本模块只持有"上一次
  *          结果"；不 include HAL，所以整条链路都能在 PC 上单测。
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include <stddef.h>

#include "calc_result.h"

#include "calc_format.h"
#include "calc_settings.h"

/* Private variables ---------------------------------------------------------*/
static calc_complex_t last_answer;      /* 上一次的结果，供 A（Ans）使用 */
static uint8_t        last_answer_valid;/* 第 2 行现在显示的是不是上一次的结果 */

/* Private functions ---------------------------------------------------------*/

/* 按当前设置格式化结果并写进一行（历史页也调它）。 */
void calc_result_format(calc_complex_t value, char line[VIEW_LINE_WIDTH])
{
  char number[CALC_FORMAT_WIDTH + 2U];
  uint8_t end;
  const uint8_t polar = settings_value(SETTING_POLAR);

  if (polar != 0U)
  {
    const calc_complex_t origin = settings_origin();

    value.real -= origin.real;
    value.imag -= origin.imag;
  }

  calc_format_complex(value, polar,
                      (settings_value(SETTING_ANGLE) == 0U) ? CALC_ANGLE_DEG : CALC_ANGLE_RAD,
                      &number[1]);
  number[CALC_FORMAT_WIDTH + 1U] = '\0';

  end = CALC_FORMAT_WIDTH;
  while ((end > 1U) && (number[end] == ' '))
  {
    number[end] = '\0';
    end--;
  }

  /* 结果正好占满 16 格时，"=" 会把最后一位（可能是复数的 'i'）挤出屏幕，
     那就不要 "=" 了，让数字自己占满整行。 */
  if (end < CALC_FORMAT_WIDTH)
  {
    number[0] = '=';
    view_set_text(line, number);
  }
  else
  {
    view_set_text(line, &number[1]);
  }
}

/* Exported functions --------------------------------------------------------*/

void calc_result_init(void)
{
  last_answer.real = 0.0f;
  last_answer.imag = 0.0f;
  last_answer_valid = 0U;
}

const char *calc_result_status_text(calc_status_t status)
{
  switch (status)
  {
    case CALC_DIV_ZERO: return "Div0 ERROR";
    case CALC_DOMAIN:   return "Math ERROR";
    case CALC_SYNTAX:   return "Syntax ERROR";
    default:            return "ERROR";
  }
}

calc_complex_t calc_result_answer(void)
{
  return last_answer;
}

void calc_result_clear_answer(void)
{
  last_answer_valid = 0U;
}

uint8_t calc_result_show_answer(char line[VIEW_LINE_WIDTH])
{
  if (last_answer_valid == 0U)
  {
    return 0U;
  }

  calc_result_format(last_answer, line);
  return 1U;
}

void calc_result_evaluate(const calc_input_t *input, calc_history_t *history,
                          uint8_t remember, char line[VIEW_LINE_WIDTH])
{
  char expression[CALC_INPUT_MAX + 1U];
  calc_complex_t result;
  calc_status_t status;
  uint8_t index;

  /* 输入缓冲不是以 '\0' 结尾的，复制一份交给求值器。 */
  for (index = 0U; index < input->length; ++index)
  {
    expression[index] = input->text[index];
  }
  expression[input->length] = '\0';

  status = calculator_evaluate(expression,
                               (settings_value(SETTING_ANGLE) == 0U) ? CALC_ANGLE_DEG : CALC_ANGLE_RAD,
                               settings_value(SETTING_COMPLEX),
                               last_answer, &result);

  switch (status)
  {
    case CALC_OK:
      last_answer = result;
      last_answer_valid = 1U;
      calc_result_format(result, line);
      if (remember != 0U)
      {
        calc_history_push(history, expression, result);
      }
      break;

    case CALC_DIV_ZERO:
      last_answer_valid = 0U;
      view_set_text(line, calc_result_status_text(CALC_DIV_ZERO));
      break;

    case CALC_DOMAIN:
      last_answer_valid = 0U;
      view_set_text(line, calc_result_status_text(CALC_DOMAIN));
      break;

    default:
      last_answer_valid = 0U;
      view_set_text(line, calc_result_status_text(CALC_SYNTAX));
      break;
  }
}

void calc_result_reapply(const calc_input_t *input, char line[VIEW_LINE_WIDTH])
{
  if (input->length == 0U)
  {
    /* 算式空着，但第 2 行还留着上一次的结果：按新设置重新格式化
       （POLAR / ANGLE 改的就是"结果怎么写"，不重画就看不出变化）。 */
    (void)calc_result_show_answer(line);
    return;
  }

  calc_result_evaluate(input, NULL, 0U, line);   /* 重算不记历史，所以 history 传 NULL */
}
