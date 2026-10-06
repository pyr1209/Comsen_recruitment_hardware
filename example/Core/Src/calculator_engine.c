/**
  ******************************************************************************
  * @file    calculator_engine.c
  * @brief   自己实现的表达式求值（支持实数与复数），取代 libcalculator_engine.a。
  *
  *          公开接口与作者的 calculator_engine.h 一致。内部统一用复数
  *          （calc_complex_t）参与运算——实数只是虚部为 0 的复数，
  *          所以实数、复数、极坐标可以混在同一个表达式里算。
  *
  *          记号（对应键盘的上档层）：
  *            p / pi = π     e = 自然常数     A = 上一次结果（Ans）
  *            i  = 虚数单位（SHIFT+9）
  *            ∠  = 极坐标构造符（SHIFT+8，CGRAM 字符码 0x01）：
  *                 a∠b = a·(cos b + i·sin b)，b 按 ANGLE UNIT 解释
  *            s( c( t(       三角函数，参数必须是实数，按 ANGLE UNIT
  *            n( / ln(       自然对数        q( / sqrt(  平方根
  *            l(a,b)         以 a 为底 b 的对数（二元，两个参数都必须填）
  *            ^              幂（右结合）
  *
  *          COMP 模式下禁止复数：出现 i 或 ∠ 直接报错；结果虚部不为 0 也报错
  *          （例如 sqrt(-4) 在 COMP 下是 Math ERROR，在 CMPLX 下是 2i）。
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include <math.h>
#include <stddef.h>

#include "calculator_engine.h"

/* Private define ------------------------------------------------------------*/
#define CALC_CHAR_END      '\0'
#define CALC_EXPONENT_MAX  38          /* float 的十进制指数上限 */
#define CALC_PI            3.14159265358979f
#define CALC_E             2.71828182845905f
#define CALC_TINY          1.0e-12f    /* 判"虚部是不是 0"用的门限 */

/* ∠ 的字符码：键盘 SHIFT+8 插的就是这个（CGRAM 自定义字符，见 lcd_cgram.c）。
   显示侧 calc_format.c 里有一份同样的定义。 */
#define CALC_CHAR_ANGLE    0x01

/* Private types -------------------------------------------------------------*/
typedef struct
{
  const char       *text;
  uint16_t          position;
  calc_status_t     status;
  calc_angle_unit_t angle_unit;
  uint8_t           allow_complex;
  calc_complex_t    answer;          /* 上一次结果，供 A(Ans) 使用 */
} calc_parser_t;

/* Private function prototypes -----------------------------------------------*/
static void  parser_skip_spaces(calc_parser_t *parser);
static char  parser_peek(const calc_parser_t *parser);
static char  parser_peek_at(const calc_parser_t *parser, uint8_t offset);

static calc_complex_t parse_expression(calc_parser_t *parser);
static calc_complex_t parse_polar(calc_parser_t *parser);
static calc_complex_t parse_term(calc_parser_t *parser);
static calc_complex_t parse_unary(calc_parser_t *parser);
static calc_complex_t parse_power(calc_parser_t *parser);
static calc_complex_t parse_primary(calc_parser_t *parser);
static calc_complex_t parse_function_call(calc_parser_t *parser, char function_id);
static calc_complex_t parse_number(calc_parser_t *parser);
static float angle_unit_to_radians(const calc_parser_t *parser);

static calc_complex_t calc_make(float real, float imag);
static calc_complex_t calc_add(calc_complex_t a, calc_complex_t b);
static calc_complex_t calc_sub(calc_complex_t a, calc_complex_t b);
static calc_complex_t calc_mul(calc_complex_t a, calc_complex_t b);
static calc_complex_t calc_div(calc_complex_t a, calc_complex_t b);
static calc_complex_t calc_neg(calc_complex_t a);
static calc_complex_t calc_exp(calc_complex_t a);
static calc_complex_t calc_log(calc_complex_t a);
static calc_complex_t calc_sqrt(calc_complex_t a);
static calc_complex_t calc_pow(calc_complex_t base, calc_complex_t exponent);
static calc_complex_t apply_function(const calc_parser_t *parser, char letter,
                                     calc_complex_t first, calc_complex_t second);
static calc_complex_t real_trig(const calc_parser_t *parser, char letter,
                                float value);
static calc_complex_t complex_trig(char letter, calc_complex_t value,
                                   float radians_per_unit);
static uint8_t complex_is_real(calc_complex_t value);
static calc_status_t parser_finish(calc_parser_t *parser, calc_complex_t value);

/* Private functions: 复数运算 ----------------------------------------------*/

static calc_complex_t calc_make(float real, float imag)
{
  calc_complex_t value;

  /* 把 -0.0 归一成 +0.0：否则 atan2(-0, -1) 会给出 -π，
     让 ln(-1) 变成 -iπ（数学上主值应该是 +iπ）。 */
  value.real = (real == 0.0f) ? 0.0f : real;
  value.imag = (imag == 0.0f) ? 0.0f : imag;
  return value;
}

static calc_complex_t calc_add(calc_complex_t a, calc_complex_t b)
{
  return calc_make(a.real + b.real, a.imag + b.imag);
}

static calc_complex_t calc_sub(calc_complex_t a, calc_complex_t b)
{
  return calc_make(a.real - b.real, a.imag - b.imag);
}

static calc_complex_t calc_mul(calc_complex_t a, calc_complex_t b)
{
  return calc_make((a.real * b.real) - (a.imag * b.imag),
                   (a.real * b.imag) + (a.imag * b.real));
}

static calc_complex_t calc_div(calc_complex_t a, calc_complex_t b)
{
  const float denominator = (b.real * b.real) + (b.imag * b.imag);

  return calc_make(((a.real * b.real) + (a.imag * b.imag)) / denominator,
                   ((a.imag * b.real) - (a.real * b.imag)) / denominator);
}

static calc_complex_t calc_neg(calc_complex_t a)
{
  return calc_make(-a.real, -a.imag);
}

static calc_complex_t calc_exp(calc_complex_t a)
{
  const float magnitude = expf(a.real);

  return calc_make(magnitude * cosf(a.imag), magnitude * sinf(a.imag));
}

static calc_complex_t calc_log(calc_complex_t a)
{
  return calc_make(logf(hypotf(a.real, a.imag)), atan2f(a.imag, a.real));
}

static calc_complex_t calc_sqrt(calc_complex_t a)
{
  const float magnitude = hypotf(a.real, a.imag);
  const float real_part = sqrtf((magnitude + a.real) * 0.5f);
  const float imag_part = sqrtf((magnitude - a.real) * 0.5f);

  if (a.imag == 0.0f)
  {
    /* 虚部为 0 时按约定位虚部非负，这样 sqrt(-4) 得到 2i 而不是 -2i。 */
    return calc_make(real_part, imag_part);
  }

  return calc_make(real_part, (a.imag < 0.0f) ? -imag_part : imag_part);
}

static calc_complex_t calc_pow(calc_complex_t base, calc_complex_t exponent)
{
  /* 正实数底 + 实数指数直接用 powf，精度最好。 */
  if ((base.imag == 0.0f) && (base.real > 0.0f) && (exponent.imag == 0.0f))
  {
    return calc_make(powf(base.real, exponent.real), 0.0f);
  }

  /* 实底数 + 小整数指数改用连乘：2^10 得到精确的 1024，更关键的是负底数
     （(-2)^3）不会因为 exp/log 的误差留下微小虚部——那点虚部会让 COMP 模式
     把一个实数结果误判成复数，直接报 Math ERROR。 */
  if ((base.imag == 0.0f) && (exponent.imag == 0.0f) &&
      (exponent.real == floorf(exponent.real)) &&
      (fabsf(exponent.real) <= 64.0f))
  {
    const uint8_t negative = (uint8_t)(exponent.real < 0.0f);
    const uint8_t steps = (uint8_t)fabsf(exponent.real);
    float value = 1.0f;
    uint8_t index;

    if ((base.real == 0.0f) && (exponent.real <= 0.0f))
    {
      return calc_make(NAN, 0.0f);      /* 0^0 和 0 的负数次幂没有定义 */
    }

    for (index = 0U; index < steps; ++index)
    {
      value *= base.real;
    }

    if (negative != 0U)
    {
      return calc_div(calc_make(1.0f, 0.0f), calc_make(value, 0.0f));
    }
    return calc_make(value, 0.0f);
  }

  return calc_exp(calc_mul(exponent, calc_log(base)));
}

/* Private functions: 其它 ---------------------------------------------------*/

static void parser_skip_spaces(calc_parser_t *parser)
{
  while ((parser->text[parser->position] == ' ') ||
         (parser->text[parser->position] == '\t'))
  {
    parser->position++;
  }
}

static char parser_peek(const calc_parser_t *parser)
{
  return parser->text[parser->position];
}

static char parser_peek_at(const calc_parser_t *parser, uint8_t offset)
{
  return parser->text[parser->position + offset];
}

static uint8_t complex_is_real(calc_complex_t value)
{
  return (uint8_t)((value.imag < CALC_TINY) && (value.imag > -CALC_TINY));
}

/* 一个角度单位里的 1 个单位等于多少弧度（DEG 时是 π/180）。 */
static float angle_unit_to_radians(const calc_parser_t *parser)
{
  if (parser->angle_unit == CALC_ANGLE_DEG)
  {
    return CALC_PI / 180.0f;
  }

  return 1.0f;
}

/**
  * @brief  加减层（最低优先级）。
  */
static calc_complex_t parse_expression(calc_parser_t *parser)
{
  calc_complex_t value = parse_term(parser);

  for (;;)
  {
    parser_skip_spaces(parser);

    {
      const char op = parser_peek(parser);
      calc_complex_t right;

      if ((op != '+') && (op != '-'))
      {
        break;
      }
      parser->position++;
      right = parse_term(parser);
      if (parser->status != CALC_OK)
      {
        return calc_make(0.0f, 0.0f);
      }
      value = (op == '+') ? calc_add(value, right) : calc_sub(value, right);
    }
  }

  return value;
}

/**
  * @brief  极坐标构造层：a∠b = a·(cos b + i·sin b)，左结合。
  * @note   优先级比乘除高（角度只吃一个操作数），比幂低：
  *           5∠30*2 = (5∠30)*2，2*3∠30 = 2*(3∠30)，2+3∠30 = 2+(3∠30)。
  */
static calc_complex_t parse_polar(calc_parser_t *parser)
{
  calc_complex_t magnitude = parse_unary(parser);

  for (;;)
  {
    parser_skip_spaces(parser);
    if (parser_peek(parser) != CALC_CHAR_ANGLE)
    {
      break;
    }
    parser->position++;

    {
      const calc_complex_t angle = parse_unary(parser);

      if (parser->status != CALC_OK)
      {
        return calc_make(0.0f, 0.0f);
      }
      if (parser->allow_complex == 0U)
      {
        parser->status = CALC_DOMAIN;    /* COMP 模式不许用极坐标 */
        return calc_make(0.0f, 0.0f);
      }
      if (complex_is_real(angle) == 0U)
      {
        parser->status = CALC_DOMAIN;    /* 角度必须是实数 */
        return calc_make(0.0f, 0.0f);
      }

      /* a∠b = a · e^{i·b}，b 先按角度单位换算成弧度 */
      magnitude = calc_mul(magnitude,
                           calc_exp(calc_make(0.0f,
                                              angle.real * angle_unit_to_radians(parser))));
    }
  }

  return magnitude;
}

/**
  * @brief  乘除层：左结合，除数为 0 报 CALC_DIV_ZERO。
  */
static calc_complex_t parse_term(calc_parser_t *parser)
{
  calc_complex_t value = parse_polar(parser);

  for (;;)
  {
    parser_skip_spaces(parser);

    {
      const char op = parser_peek(parser);
      calc_complex_t right;

      if ((op != '*') && (op != '/'))
      {
        break;
      }
      parser->position++;
      right = parse_polar(parser);
      if (parser->status != CALC_OK)
      {
        return calc_make(0.0f, 0.0f);
      }
      if (op == '*')
      {
        value = calc_mul(value, right);
      }
      else
      {
        if ((right.real == 0.0f) && (right.imag == 0.0f))
        {
          parser->status = CALC_DIV_ZERO;
          return calc_make(0.0f, 0.0f);
        }
        value = calc_div(value, right);
      }
    }
  }

  return value;
}

/**
  * @brief  一元正负号（在幂的外面，所以 -2^2 = -4）。
  */
static calc_complex_t parse_unary(calc_parser_t *parser)
{
  parser_skip_spaces(parser);

  if (parser_peek(parser) == '+')
  {
    parser->position++;
    return parse_unary(parser);
  }
  if (parser_peek(parser) == '-')
  {
    parser->position++;
    return calc_neg(parse_unary(parser));
  }

  return parse_power(parser);
}

/**
  * @brief  幂：右结合，指数可带一元负号。
  */
static calc_complex_t parse_power(calc_parser_t *parser)
{
  calc_complex_t value = parse_primary(parser);

  if (parser->status != CALC_OK)
  {
    return calc_make(0.0f, 0.0f);
  }

  parser_skip_spaces(parser);
  if (parser_peek(parser) == '^')
  {
    calc_complex_t exponent;

    parser->position++;
    exponent = parse_unary(parser);
    if (parser->status != CALC_OK)
    {
      return calc_make(0.0f, 0.0f);
    }
    value = calc_pow(value, exponent);
  }

  return value;
}

/**
  * @brief  基本项：括号、函数、常量、数字。
  */
static calc_complex_t parse_primary(calc_parser_t *parser)
{
  char letter;

  parser_skip_spaces(parser);

  if (parser_peek(parser) == '(')
  {
    calc_complex_t value;

    parser->position++;
    value = parse_expression(parser);
    if (parser->status != CALC_OK)
    {
      return calc_make(0.0f, 0.0f);
    }

    parser_skip_spaces(parser);
    if (parser_peek(parser) != ')')
    {
      parser->status = CALC_SYNTAX;
      return calc_make(0.0f, 0.0f);
    }
    parser->position++;
    return value;
  }

  letter = parser_peek(parser);

  /* 多字母记号优先：pi / ln / sqrt（否则 ln( 会被当成函数 l） */
  if ((letter == 'p') && (parser_peek_at(parser, 1U) == 'i'))
  {
    parser->position += 2U;
    return calc_make(CALC_PI, 0.0f);
  }
  if ((letter == 'l') && (parser_peek_at(parser, 1U) == 'n'))
  {
    parser->position += 2U;
    return parse_function_call(parser, 'n');
  }
  if ((letter == 's') && (parser_peek_at(parser, 1U) == 'q') &&
      (parser_peek_at(parser, 2U) == 'r') && (parser_peek_at(parser, 3U) == 't'))
  {
    parser->position += 4U;
    return parse_function_call(parser, 'q');
  }

  /* 虚数单位：COMP 模式下不许出现 */
  if (letter == 'i')
  {
    if (parser->allow_complex == 0U)
    {
      parser->status = CALC_DOMAIN;
      return calc_make(0.0f, 0.0f);
    }
    parser->position++;
    return calc_make(0.0f, 1.0f);
  }

  if ((letter == 'c') || (letter == 'l') || (letter == 'n') ||
      (letter == 'q') || (letter == 's') || (letter == 't'))
  {
    parser->position++;
    return parse_function_call(parser, letter);
  }

  if (letter == 'p')
  {
    parser->position++;
    return calc_make(CALC_PI, 0.0f);
  }
  if (letter == 'e')
  {
    parser->position++;
    return calc_make(CALC_E, 0.0f);
  }
  if (letter == 'A')
  {
    parser->position++;
    return parser->answer;
  }

  return parse_number(parser);
}

/**
  * @brief  解析函数参数 ( 参数 [, 参数] )，再按 function_id 求值。
  */
static calc_complex_t parse_function_call(calc_parser_t *parser, char function_id)
{
  calc_complex_t first;
  calc_complex_t second = {0.0f, 0.0f};
  uint8_t two_arguments = 0U;

  parser_skip_spaces(parser);
  if (parser_peek(parser) != '(')
  {
    parser->status = CALC_SYNTAX;
    return calc_make(0.0f, 0.0f);
  }
  parser->position++;

  first = parse_expression(parser);
  if (parser->status != CALC_OK)
  {
    return calc_make(0.0f, 0.0f);
  }

  parser_skip_spaces(parser);
  if (parser_peek(parser) == ',')
  {
    parser->position++;
    second = parse_expression(parser);
    if (parser->status != CALC_OK)
    {
      return calc_make(0.0f, 0.0f);
    }
    two_arguments = 1U;
    parser_skip_spaces(parser);
  }

  if (parser_peek(parser) != ')')
  {
    parser->status = CALC_SYNTAX;
    return calc_make(0.0f, 0.0f);
  }
  parser->position++;

  /* log 是二元函数：必须写满 l(底数,真数)。留了空槽（l(2,)）或只给一个参数
     （l(100)）都报语法错——不替使用者假定"没填的那个数是 10"。 */
  if ((function_id == 'l') && (two_arguments == 0U))
  {
    parser->status = CALC_SYNTAX;
    return calc_make(0.0f, 0.0f);
  }

  return apply_function(parser, function_id, first, second);
}

/**
  * @brief  实参数的三角函数（原路径）：角度按当前单位换算，tan 判极点。
  */
static calc_complex_t real_trig(const calc_parser_t *parser, char letter,
                                float value)
{
  const float angle = value * angle_unit_to_radians(parser);

  if (letter == 't')
  {
    /* 正切在极点处无定义，显式判掉（例如 DEG 下 t(90)） */
    const float period = (parser->angle_unit == CALC_ANGLE_DEG) ? 180.0f : CALC_PI;
    const float pole = period * 0.5f;
    float normalized = fmodf(value, period);

    if (normalized < 0.0f)
    {
      normalized += period;
    }
    if (fabsf(normalized - pole) < (period * 1.0e-6f))
    {
      return calc_make(NAN, 0.0f);
    }
  }

  if (letter == 's')
  {
    return calc_make(sinf(angle), 0.0f);
  }
  if (letter == 'c')
  {
    return calc_make(cosf(angle), 0.0f);
  }
  return calc_make(tanf(angle), 0.0f);
}

/**
  * @brief  复参数的三角函数：sin/cos 按实部虚部展开，tan = sin / cos。
  * @note   sin(a+bi) = sin a·cosh b + i·cos a·sinh b
  *         cos(a+bi) = cos a·cosh b − i·sin a·sinh b
  *         角度单位只换算实部：虚部不是一个"角度"，所以 sin(i) 在 DEG / RAD
  *         下结果相同。b ≠ 0 时 cos(a+bi) 不可能为 0，tan 不会有极点。
  */
static calc_complex_t complex_trig(char letter, calc_complex_t value,
                                   float radians_per_unit)
{
  const float a = value.real * radians_per_unit;   /* DEG 时换算成弧度 */
  const float b = value.imag;
  const float cosh_b = (expf(b) + expf(-b)) * 0.5f;
  const float sinh_b = (expf(b) - expf(-b)) * 0.5f;
  const calc_complex_t sine = calc_make(sinf(a) * cosh_b, cosf(a) * sinh_b);
  const calc_complex_t cosine = calc_make(cosf(a) * cosh_b, -sinf(a) * sinh_b);

  if (letter == 's')
  {
    return sine;
  }
  if (letter == 'c')
  {
    return cosine;
  }
  return calc_div(sine, cosine);
}

/**
  * @brief  按函数标识求值。
  * @note   三角函数、对数、根号、幂都支持复数参数（复变函数）。
  */
static calc_complex_t apply_function(const calc_parser_t *parser, char letter,
                                     calc_complex_t first, calc_complex_t second)
{
  const float radians_per_unit = angle_unit_to_radians(parser);

  switch (letter)
  {
    case 's':
    case 'c':
    case 't':
    {
      /* 实参数走原来的路子（单位换算 + tan 极点特判），
         虚部非 0 时按复变函数的定义展开。 */
      if (complex_is_real(first) != 0U)
      {
        return real_trig(parser, letter, first.real);
      }
      return complex_trig(letter, first, radians_per_unit);
    }

    case 'n':
      return calc_log(first);             /* 自然对数（支持负数 → 复数） */

    case 'q':
      return calc_sqrt(first);            /* 平方根（负数 → 虚数） */

    default:                              /* 'l'：二元对数，以 first 为底 */
      return calc_div(calc_log(second), calc_log(first));
  }
}

/**
  * @brief  解析一个十进制数字，返回实部为该值的复数。
  */
static calc_complex_t parse_number(calc_parser_t *parser)
{
  float value = 0.0f;
  float scale = 0.1f;
  uint8_t has_digit = 0U;

  parser_skip_spaces(parser);

  while ((parser_peek(parser) >= '0') && (parser_peek(parser) <= '9'))
  {
    value = (value * 10.0f) + (float)(parser_peek(parser) - '0');
    parser->position++;
    has_digit = 1U;
  }

  if (parser_peek(parser) == '.')
  {
    parser->position++;

    while ((parser_peek(parser) >= '0') && (parser_peek(parser) <= '9'))
    {
      value += scale * (float)(parser_peek(parser) - '0');
      scale *= 0.1f;
      parser->position++;
      has_digit = 1U;
    }
  }

  if (has_digit == 0U)
  {
    parser->status = CALC_SYNTAX;
    return calc_make(0.0f, 0.0f);
  }

  if (parser_peek(parser) == 'E')
  {
    int16_t exponent = 0;
    uint8_t exponent_digit = 0U;
    uint8_t negative = 0U;
    float factor = 1.0f;

    parser->position++;

    if (parser_peek(parser) == '+')
    {
      parser->position++;
    }
    else if (parser_peek(parser) == '-')
    {
      negative = 1U;
      parser->position++;
    }

    while ((parser_peek(parser) >= '0') && (parser_peek(parser) <= '9'))
    {
      if (exponent < 100)
      {
        exponent = (int16_t)((exponent * 10) + (parser_peek(parser) - '0'));
      }
      parser->position++;
      exponent_digit = 1U;
    }

    if ((exponent_digit == 0U) || (exponent > CALC_EXPONENT_MAX))
    {
      parser->status = (exponent_digit == 0U) ? CALC_SYNTAX : CALC_DOMAIN;
      return calc_make(0.0f, 0.0f);
    }

    while (exponent > 0)
    {
      factor *= 10.0f;
      exponent--;
    }

    value = (negative != 0U) ? (value / factor) : (value * factor);
  }

  return calc_make(value, 0.0f);
}

/**
  * @brief  收尾检查：语法残留 + 结果是否有限 + COMP 模式下的复数。
  */
static calc_status_t parser_finish(calc_parser_t *parser, calc_complex_t value)
{
  if (parser->status != CALC_OK)
  {
    return parser->status;
  }

  parser_skip_spaces(parser);
  if (parser_peek(parser) != CALC_CHAR_END)
  {
    return CALC_SYNTAX;
  }

  /* NaN 或超出 float 范围 */
  if (!((value.real == value.real) && (value.real <= 3.4e38f) &&
        (value.real >= -3.4e38f) &&
        (value.imag == value.imag) && (value.imag <= 3.4e38f) &&
        (value.imag >= -3.4e38f)))
  {
    return CALC_DOMAIN;
  }

  /* COMP 模式下结果必须是实数 */
  if ((parser->allow_complex == 0U) && (complex_is_real(value) == 0U))
  {
    return CALC_DOMAIN;
  }

  return CALC_OK;
}

/* Exported functions --------------------------------------------------------*/

calc_status_t calculator_evaluate(const char *expression,
                                  calc_angle_unit_t angle_unit,
                                  uint8_t allow_complex,
                                  calc_complex_t answer,
                                  calc_complex_t *result)
{
  calc_parser_t parser;
  calc_complex_t value;
  calc_status_t status;

  if ((expression == NULL) || (result == NULL))
  {
    return CALC_SYNTAX;
  }

  parser.text = expression;
  parser.position = 0U;
  parser.status = CALC_OK;
  parser.angle_unit = angle_unit;
  parser.allow_complex = allow_complex;
  parser.answer = answer;

  value = parse_expression(&parser);
  status = parser_finish(&parser, value);

  if (status == CALC_OK)
  {
    result->real = value.real;
    result->imag = value.imag;
  }

  return status;
}

calc_status_t calculator_solve_linear(float a, float b, float *x)
{
  if (x == NULL)
  {
    return CALC_SYNTAX;
  }

  if (a == 0.0f)
  {
    return CALC_DOMAIN;
  }

  *x = -b / a;
  return CALC_OK;
}
