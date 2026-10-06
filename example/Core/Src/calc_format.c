/**
  ******************************************************************************
  * @file    calc_format.c
  * @brief   把结果格式化成 1602 一行的 16 个字符（实数 / 复数）。
  *
  *          自己实现，避免为了 %f 引入 printf 的浮点格式化
  *          （nano 库要加 -u _printf_float，会多占两三千字节 Flash）。
  *
  *          实数：整数部分 + 最多 6 位小数，去尾随 0；太大/太小用科学计数法；
  *                绝对值小于 5e-7 显示成 0（消掉浮点残差）。
  *          复数：RECT  → a+bi     POLAR → r∠θ（θ 按角度单位换算）
  *                实部或虚部为 0 时不写出来（4i、而不是 0+4i；
  *                极坐标下的纯实数仍按普通实数显示，不写 r∠0）。
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include <math.h>

#include "calc_format.h"

/* Private define ------------------------------------------------------------*/
#define FORMAT_MAX_DECIMALS   6U
#define FORMAT_SIGNIFICANT    7U        /* float 大约只有 7 位有效十进制数字 */
#define FORMAT_FIXED_LIMIT    1.0e7f    /* 超过这个量级改用科学计数法 */
#define FORMAT_ZERO_LIMIT     5.0e-7f   /* 小于它就当作 0 */
#define FORMAT_POW10_COUNT    39U
#define FORMAT_REAL_PART_WIDTH 6U       /* 复数的实部 / 模 占几格 */
#define FORMAT_IMAG_PART_WIDTH 7U       /* 虚部占几格（多一位留给负号/小数） */
#define FORMAT_ANGLE_PART_WIDTH 8U      /* 极坐标里的角度占几格 */
/* 压缩复数的两部分时，每一份最少留几格：7 格刚好写 "1.2E+06" 这种科学计数法，
   再窄就写不出有效数字了，宁可就到这里为止。 */
#define FORMAT_PART_MIN_WIDTH  7U
#define FORMAT_REAL_LIMIT     1.0e-6f   /* 虚部小于它就认为结果是实数 */

/* ∠ 的字符码：CGRAM 自定义字符（见 lcd_cgram.c）；引擎里也有一份同样的定义。 */
#define FORMAT_ANGLE_CHAR     0x01

/* Private variables ---------------------------------------------------------*/
static const float pow10_table[FORMAT_POW10_COUNT] =
{
  1.0e0f,  1.0e1f,  1.0e2f,  1.0e3f,  1.0e4f,  1.0e5f,  1.0e6f,
  1.0e7f,  1.0e8f,  1.0e9f,  1.0e10f, 1.0e11f, 1.0e12f, 1.0e13f,
  1.0e14f, 1.0e15f, 1.0e16f, 1.0e17f, 1.0e18f, 1.0e19f, 1.0e20f,
  1.0e21f, 1.0e22f, 1.0e23f, 1.0e24f, 1.0e25f, 1.0e26f, 1.0e27f,
  1.0e28f, 1.0e29f, 1.0e30f, 1.0e31f, 1.0e32f, 1.0e33f, 1.0e34f,
  1.0e35f, 1.0e36f, 1.0e37f, 1.0e38f
};

/* Private function prototypes -----------------------------------------------*/
static void    text_fill(char *text, uint8_t width, char character);
static uint8_t append_text(char *text, uint8_t width, uint8_t position,
                           const char *source);
static uint8_t append_char(char *text, uint8_t width, uint8_t position,
                           char character);
static uint8_t append_chars(char *text, uint8_t width, uint8_t position,
                            const char *source, uint8_t length);
static uint8_t count_digits(uint32_t value);
static uint8_t append_digits(char *text, uint8_t width, uint8_t position,
                             uint32_t value, uint8_t digits);
static uint8_t append_fraction(char *text, uint8_t width, uint8_t position,
                               uint32_t fraction, uint8_t decimals);
static uint8_t format_real(float value, uint8_t width, char *text);
static uint8_t format_imaginary(float value, uint8_t width, char *text);
static uint8_t text_length(const char *text, uint8_t width);

/* Private functions ---------------------------------------------------------*/

static void text_fill(char *text, uint8_t width, char character)
{
  uint8_t index;

  for (index = 0U; index < width; ++index)
  {
    text[index] = character;
  }
}

static uint8_t append_text(char *text, uint8_t width, uint8_t position,
                           const char *source)
{
  while ((*source != '\0') && (position < width))
  {
    text[position] = *source;
    position++;
    source++;
  }

  return position;
}

static uint8_t append_char(char *text, uint8_t width, uint8_t position,
                           char character)
{
  if (position < width)
  {
    text[position] = character;
    position++;
  }

  return position;
}

/* 按给定长度拼接（源串不是 '\0' 结尾的，比如右侧补空格的数字字段） */
static uint8_t append_chars(char *text, uint8_t width, uint8_t position,
                            const char *source, uint8_t length)
{
  uint8_t index;

  for (index = 0U; index < length; ++index)
  {
    position = append_char(text, width, position, source[index]);
  }

  return position;
}

static uint8_t count_digits(uint32_t value)
{
  uint8_t digits = 1U;

  while (value >= 10U)
  {
    value /= 10U;
    digits++;
  }

  return digits;
}

static uint8_t append_digits(char *text, uint8_t width, uint8_t position,
                             uint32_t value, uint8_t digits)
{
  uint32_t divisor = 1U;
  uint8_t index;

  for (index = 1U; index < digits; ++index)
  {
    divisor *= 10U;
  }

  for (index = 0U; index < digits; ++index)
  {
    position = append_char(text, width, position,
                           (char)('0' + ((value / divisor) % 10U)));
    if (divisor >= 10U)
    {
      divisor /= 10U;
    }
  }

  return position;
}

/**
  * @brief  写小数点和小数部分，自动去掉尾随 0。
  */
static uint8_t append_fraction(char *text, uint8_t width, uint8_t position,
                               uint32_t fraction, uint8_t decimals)
{
  uint32_t trimmed = fraction;
  uint8_t keep = decimals;
  uint8_t digit_index;

  if ((decimals == 0U) || (fraction == 0U))
  {
    return position;
  }

  while ((keep > 0U) && ((trimmed % 10U) == 0U))
  {
    trimmed /= 10U;
    keep--;
  }

  if (keep == 0U)
  {
    return position;
  }

  position = append_char(text, width, position, '.');

  {
    uint32_t scale = 1U;

    for (digit_index = 1U; digit_index < decimals; ++digit_index)
    {
      scale *= 10U;
    }

    for (digit_index = 0U; digit_index < keep; ++digit_index)
    {
      position = append_char(text, width, position,
                             (char)('0' + ((fraction / scale) % 10U)));
      if (scale >= 10U)
      {
        scale /= 10U;
      }
    }
  }

  return position;
}

/**
  * @brief  把一个实数写进 width 格（右侧空格补齐）。
  * @retval 去掉尾随空格后的实际长度
  */
static uint8_t format_real(float value, uint8_t width, char *text)
{
  uint8_t position = 0U;
  uint8_t negative = 0U;
  float magnitude;
  uint32_t whole;
  uint8_t whole_digits;
  uint8_t decimals;
  uint32_t fraction;
  float factor;
  uint8_t index;
  int16_t exponent = 0;

  text_fill(text, width, ' ');

  if ((value != value) || (value > 3.4e38f) || (value < -3.4e38f))
  {
    (void)append_text(text, width, 0U, "Math ERROR");
    return text_length(text, width);
  }

  if ((value < FORMAT_ZERO_LIMIT) && (value > -FORMAT_ZERO_LIMIT))
  {
    value = 0.0f;                        /* 消掉浮点残差 */
  }

  if (value < 0.0f)
  {
    negative = 1U;
    magnitude = -value;
  }
  else
  {
    magnitude = value;
  }

  if (magnitude == 0.0f)
  {
    negative = 0U;                       /* 避免 -0 */
  }

  /* 太大或太小 → 科学计数法 d[.ddd]E±ee；
     另外，整数部分在当前宽度里放不下时也必须走科学计数法——定点那条路
     是按宽度逐个塞数字的，硬走会把高位数字砍掉（显示出一个错的数）。 */
  if ((magnitude != 0.0f) &&
      ((magnitude >= FORMAT_FIXED_LIMIT) || (magnitude < 1.0e-4f) ||
       (((uint16_t)negative + (uint16_t)count_digits((uint32_t)magnitude)) > width)))
  {
    uint8_t exponent_digits = 2U;

    if (magnitude >= 1.0f)
    {
      uint8_t power = 0U;

      while ((power < (FORMAT_POW10_COUNT - 1U)) &&
             (magnitude >= pow10_table[power + 1U]))
      {
        power++;
      }
      magnitude /= pow10_table[power];
      exponent = (int16_t)power;
    }
    else
    {
      uint8_t power = 0U;

      while ((power < (FORMAT_POW10_COUNT - 1U)) &&
             ((magnitude * pow10_table[power]) < 1.0f))
      {
        power++;
      }
      magnitude *= pow10_table[power];
      exponent = (int16_t)(-(int16_t)power);
    }

    if (magnitude >= 9.99999f)
    {
      magnitude = 1.0f;
      exponent++;
    }

    if (position < width && negative != 0U)
    {
      position = append_char(text, width, position, '-');
    }

    whole = (uint32_t)magnitude;
    fraction = (uint32_t)(((magnitude - (float)whole) * 1000.0f) + 0.5f);
    if (fraction >= 1000U)
    {
      fraction -= 1000U;
      whole++;
      if (whole >= 10U)
      {
        whole = 1U;
        exponent++;
      }
    }

    /* 指数占几位（最多 3 位；我们的范围不超过 ±38，所以通常是 2 位） */
    if ((exponent >= 100) || (exponent <= -100))
    {
      exponent_digits = 3U;
    }

    /* 宽度不够时先砍小数位 */
    {
      uint8_t available = (uint8_t)(position + 1U + 1U + 1U + 1U + exponent_digits);

      if (available < width)
      {
        const uint8_t room = (uint8_t)(width - available);
        uint8_t keep = 3U;
        uint32_t trimmed = fraction;

        while ((keep > 0U) && ((trimmed % 10U) == 0U))
        {
          trimmed /= 10U;
          keep--;
        }
        if (keep > room)
        {
          keep = room;
        }

        position = append_char(text, width, position, (char)('0' + whole));
        if (keep > 0U)
        {
          uint32_t divisor = 100U;
          uint8_t digit_index;

          position = append_char(text, width, position, '.');
          for (digit_index = 0U; digit_index < keep; ++digit_index)
          {
            position = append_char(text, width, position,
                                   (char)('0' + ((fraction / divisor) % 10U)));
            if (divisor >= 10U)
            {
              divisor /= 10U;
            }
          }
        }
      }
      else
      {
        position = append_char(text, width, position, (char)('0' + whole));
      }
    }

    position = append_char(text, width, position, 'E');
    if (exponent < 0)
    {
      position = append_char(text, width, position, '-');
      exponent = (int16_t)(-exponent);
    }
    else
    {
      position = append_char(text, width, position, '+');
    }

    if (exponent >= 100)
    {
      position = append_digits(text, width, position, (uint32_t)(exponent / 100), 1U);
      exponent = (int16_t)(exponent % 100);
    }
    position = append_digits(text, width, position, (uint32_t)(exponent / 10), 1U);
    position = append_digits(text, width, position, (uint32_t)(exponent % 10), 1U);

    return text_length(text, width);
  }

  /* 定点表示 */
  whole = (uint32_t)magnitude;
  whole_digits = count_digits(whole);

  if (whole_digits < FORMAT_SIGNIFICANT)
  {
    decimals = (uint8_t)(FORMAT_SIGNIFICANT - whole_digits);
    if (decimals > FORMAT_MAX_DECIMALS)
    {
      decimals = FORMAT_MAX_DECIMALS;
    }
  }
  else
  {
    decimals = 0U;
  }

  {
    const uint16_t used = (uint16_t)negative + (uint16_t)whole_digits + 1U;

    if ((used + (uint16_t)decimals) > width)
    {
      decimals = (width > used) ? (uint8_t)(width - used) : 0U;
    }
  }

  factor = 1.0f;
  for (index = 0U; index < decimals; ++index)
  {
    factor *= 10.0f;
  }

  fraction = (uint32_t)(((magnitude - (float)whole) * factor) + 0.5f);
  if ((decimals > 0U) && (fraction >= (uint32_t)factor))
  {
    fraction -= (uint32_t)factor;
    whole++;
    whole_digits = count_digits(whole);
  }

  if (negative != 0U)
  {
    position = append_char(text, width, position, '-');
  }

  position = append_digits(text, width, position, whole, whole_digits);
  position = append_fraction(text, width, position, fraction, decimals);

  return text_length(text, width);
}

/*
 * 虚部的"系数"部分（不带符号）：格式和实数一样，只有一点不同——
 * |imag| 正好是 1 时系数留空，让调用者补一个 'i'，写成 -i / i / 3-i，
 * 而不是 -1i / 1i / 3-1i。判等用和"当作 0"同一档容差，避免浮点残差。
 */
static uint8_t format_imaginary(float value, uint8_t width, char *text)
{
  const float magnitude = (value < 0.0f) ? -value : value;

  if ((magnitude > (1.0f - FORMAT_REAL_LIMIT)) &&
      (magnitude < (1.0f + FORMAT_REAL_LIMIT)))
  {
    text_fill(text, width, ' ');
    return 0U;
  }

  return format_real(magnitude, width, text);
}

static uint8_t text_length(const char *text, uint8_t width)
{
  uint8_t length = width;

  while ((length > 0U) && (text[length - 1U] == ' '))
  {
    length--;
  }

  return length;
}

/* Exported functions --------------------------------------------------------*/

void calc_format_float(float value, char text[CALC_FORMAT_WIDTH])
{
  (void)format_real(value, CALC_FORMAT_WIDTH, text);
}

void calc_format_complex(calc_complex_t value, uint8_t polar,
                         calc_angle_unit_t angle_unit,
                         char text[CALC_FORMAT_WIDTH])
{
  char part[CALC_FORMAT_WIDTH];
  char second[CALC_FORMAT_WIDTH];
  uint8_t part_width;
  uint8_t second_width;
  uint8_t part_length;
  uint8_t second_length;
  uint8_t position;

  /* 虚部为 0（或极小）就按普通实数显示，两种模式下都一样。 */
  if ((value.imag < FORMAT_REAL_LIMIT) && (value.imag > -FORMAT_REAL_LIMIT))
  {
    (void)format_real(value.real, CALC_FORMAT_WIDTH, text);
    return;
  }

  text_fill(text, CALC_FORMAT_WIDTH, ' ');

  if (polar != 0U)
  {
    /* POLAR：r∠θ，θ 按角度单位换算 */
    float angle = atan2f(value.imag, value.real);

    if (angle_unit == CALC_ANGLE_DEG)
    {
      angle = angle * (180.0f / 3.14159265358979f);
    }

    /* 先各按整行宽度排，放得下就都用全精度；放不下才一点点压更长的那一边。
       固定 6/8 格会让 1048576 这种结果悄悄丢高位数字，所以必须自适应。 */
    part_width = CALC_FORMAT_WIDTH - 2U;    /* 给 ∠ 和至少一格角度留位置 */
    second_width = CALC_FORMAT_WIDTH - 2U;
    for (;;)
    {
      part_length = format_real(hypotf(value.real, value.imag), part_width, part);
      second_length = format_real(angle, second_width, second);
      if (((uint16_t)part_length + second_length + 1U) <= CALC_FORMAT_WIDTH)
      {
        break;
      }
      if ((second_length >= part_length) && (second_width > FORMAT_PART_MIN_WIDTH))
      {
        second_width--;
      }
      else if (part_width > FORMAT_PART_MIN_WIDTH)
      {
        part_width--;
      }
      else
      {
        break;
      }
    }

    position = append_chars(text, CALC_FORMAT_WIDTH, 0U, part, part_length);
    position = append_char(text, CALC_FORMAT_WIDTH, position, FORMAT_ANGLE_CHAR);
    (void)append_chars(text, CALC_FORMAT_WIDTH, position, second, second_length);
    return;
  }

  /* RECT：a+bi */
  if ((value.real < FORMAT_REAL_LIMIT) && (value.real > -FORMAT_REAL_LIMIT))
  {
    /* 纯虚数：只写虚部，例如 4i / -4i / i / -i */
    position = 0U;
    if (value.imag < 0.0f)
    {
      position = append_char(text, CALC_FORMAT_WIDTH, position, '-');
    }
    second_length = format_imaginary(value.imag, CALC_FORMAT_WIDTH - 2U, second);
    position = append_chars(text, CALC_FORMAT_WIDTH, position, second, second_length);
    (void)append_char(text, CALC_FORMAT_WIDTH, position, 'i');
    return;
  }

  /* 同上：实部 / 虚部按实际长度分这 16 格，数字放得下就不许被截断。 */
  part_width = CALC_FORMAT_WIDTH - 2U;      /* 给符号和 'i' 留两格 */
  second_width = CALC_FORMAT_WIDTH - 2U;
  for (;;)
  {
    part_length = format_real(value.real, part_width, part);
    second_length = format_imaginary(value.imag, second_width, second);
    if (((uint16_t)part_length + second_length + 2U) <= CALC_FORMAT_WIDTH)
    {
      break;
    }
    if ((second_length >= part_length) && (second_width > FORMAT_PART_MIN_WIDTH))
    {
      second_width--;
    }
    else if (part_width > FORMAT_PART_MIN_WIDTH)
    {
      part_width--;
    }
    else
    {
      break;
    }
  }

  position = append_chars(text, CALC_FORMAT_WIDTH, 0U, part, part_length);
  position = append_char(text, CALC_FORMAT_WIDTH, position,
                         (value.imag < 0.0f) ? '-' : '+');
  position = append_chars(text, CALC_FORMAT_WIDTH, position, second, second_length);
  (void)append_char(text, CALC_FORMAT_WIDTH, position, 'i');
}
