/**
  ******************************************************************************
  * @file    lcd_cgram.c
  * @brief   1602 自定义字符（CGRAM）。
  *
  *          为什么需要它：1602 的字符 ROM 里没有 ∠、° 这类符号，
  *          想显示只能往 CGRAM 里写点阵。作者的 LCD 库没暴露这个接口，
  *          所以这里按它的接线和时序自己发几个字节。
  *
  *          引脚（反汇编作者库得到，与 MX_GPIO_Init 一致）：
  *            RS = PB10，E = PB11，D4-D7 = PB12-PB15
  *          协议：4 位模式，每个字节先发高半字节再发低半字节；
  *                每半字节：数据放到 D4-D7 并设置 RS → E 拉高 → 延时
  *                          → E 拉低 → 延时。
  *          时基用 TIM3 的 CNT（1 MHz），和作者库、触摸驱动一致。
  *
  *          注意：CGRAM 掉电即失，所以每次上电都要重新定义一次。
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "lcd_cgram.h"

#include "lcd1602.h"
#include "main.h"

/* Private define ------------------------------------------------------------*/
#define LCD_CGRAM_PORT     GPIOB
#define LCD_CGRAM_RS_PIN   GPIO_PIN_10
#define LCD_CGRAM_E_PIN    GPIO_PIN_11
/* D4..D7 = PB12..PB15 */

#define LCD_CMD_SET_CGRAM_ADDRESS 0x40U
#define LCD_CMD_SET_DDRAM_ADDRESS 0x80U

/* Private variables ---------------------------------------------------------*/
extern TIM_HandleTypeDef htim3;

/* Private function prototypes -----------------------------------------------*/
static void cgram_delay_us(uint16_t microseconds);
static void cgram_write_nibble(uint8_t nibble, uint8_t rs);
static void cgram_write_byte(uint8_t value, uint8_t rs);

/* Private functions ---------------------------------------------------------*/

static void cgram_delay_us(uint16_t microseconds)
{
  const uint16_t start = (uint16_t)__HAL_TIM_GET_COUNTER(&htim3);

  while ((uint16_t)((uint16_t)__HAL_TIM_GET_COUNTER(&htim3) - start) < microseconds)
  {
  }
}

/**
  * @brief  发半个字节（4 位数据 + RS），带一个 E 脉冲。
  * @note   BSRR 的高 16 位负责把为 0 的数据位和 RS 拉低，
  *         这样就不用读-改-写，和作者库的做法一致。
  */
static void cgram_write_nibble(uint8_t nibble, uint8_t rs)
{
  uint32_t set_bits = ((uint32_t)nibble << 12);                       /* PB12-PB15 置位 */
  uint32_t reset_bits = ((uint32_t)(~nibble) & 0x0FU) << 28U;         /* 为 0 的位复位 */

  if (rs != 0U)
  {
    set_bits |= LCD_CGRAM_RS_PIN;
  }
  else
  {
    reset_bits |= (uint32_t)LCD_CGRAM_RS_PIN << 16U;
  }

  LCD_CGRAM_PORT->BSRR = set_bits | reset_bits;
  cgram_delay_us(1U);

  LCD_CGRAM_PORT->BSRR = LCD_CGRAM_E_PIN;                             /* E 拉高 */
  cgram_delay_us(1U);

  LCD_CGRAM_PORT->BSRR = (uint32_t)LCD_CGRAM_E_PIN << 16U;            /* E 拉低 */
  cgram_delay_us(1U);
}

/**
  * @brief  发一个字节：先高半字节，再低半字节。
  */
static void cgram_write_byte(uint8_t value, uint8_t rs)
{
  cgram_write_nibble((uint8_t)(value >> 4), rs);
  cgram_write_nibble((uint8_t)(value & 0x0FU), rs);
  cgram_delay_us(60U);
}

/* Exported functions --------------------------------------------------------*/

void lcd_cgram_define(uint8_t slot, const uint8_t pattern[8])
{
  uint8_t index;

  /* 先把地址指针指到 CGRAM 的第 slot 格，然后连续写 8 个字节的点阵。 */
  cgram_write_byte((uint8_t)(LCD_CMD_SET_CGRAM_ADDRESS | (uint8_t)(slot << 3U)), 0U);

  for (index = 0U; index < 8U; ++index)
  {
    cgram_write_byte(pattern[index], 1U);
  }

  /* 把地址指针放回第一行开头，免得影响作者库后面的整屏写。 */
  cgram_write_byte(LCD_CMD_SET_DDRAM_ADDRESS, 0U);
}

void lcd_cgram_define_angle(void)
{
  /* ∠：一条从右上到左下的斜线 + 一条底边，顶点在左下角。 */
  static const uint8_t angle_pattern[8] =
  {
    0x01U,   /* ....# */
    0x02U,   /* ...#. */
    0x04U,   /* ..#.. */
    0x08U,   /* .#... */
    0x10U,   /* #.... */
    0x1FU,   /* ##### */
    0x00U,
    0x00U
  };

  lcd_cgram_define(LCD1602_CHAR_ANGLE, angle_pattern);
}

/*
 * 小游戏的精灵点阵。每行低 5 位是 5 个像素，bit0 在最右边，例如
 * 0x04 = "..#.."、0x1F = "#####"。每行后面都标了图案，方便对着改。
 */
void lcd_cgram_define_game_sprites(void)
{
  /* 恐龙（朝右）：头在上，眼睛是头中间那个洞，尾巴朝左，下面是两条腿 */
  static const uint8_t dino_run1[8] =
  {
    0x0CU,   /* ..##. */
    0x07U,   /* ..### */
    0x05U,   /* ..#.# */
    0x0CU,   /* ..##. */
    0x0FU,   /* .#### */
    0x1FU,   /* ##### */
    0x0AU,   /* .#.#. */
    0x09U    /* .#..# */
  };

  /* 跑动第 2 帧：两条腿收到中间，和上一帧交替起来就有跑动的感觉 */
  static const uint8_t dino_run2[8] =
  {
    0x0CU, 0x07U, 0x05U, 0x0CU, 0x0FU, 0x1FU,
    0x0CU,   /* ..##. */
    0x04U    /* ..#.. */
  };

  /* 腾空：上半身不变，腿收起来、最下面一行留空，看着就是跳起来了 */
  static const uint8_t dino_jump[8] =
  {
    0x0CU, 0x07U, 0x05U, 0x0CU, 0x0FU, 0x1FU,
    0x0CU,   /* ..##. */
    0x00U    /* ..... */
  };

  /* 仙人掌：中间一根主干，左右各伸一条手臂，底部张开像扎在地上 */
  static const uint8_t cactus[8] =
  {
    0x04U,   /* ..#.. */
    0x04U,   /* ..#.. */
    0x15U,   /* #.#.# */
    0x1BU,   /* ##.## */
    0x04U,   /* ..#.. */
    0x04U,   /* ..#.. */
    0x04U,   /* ..#.. */
    0x0EU    /* .###. */
  };

  /* 云：只占上半部分，在第 1 行飘过去当背景 */
  static const uint8_t cloud[8] =
  {
    0x00U,   /* ..... */
    0x0EU,   /* .###. */
    0x1FU,   /* ##### */
    0x1EU,   /* ####. */
    0x00U, 0x00U, 0x00U, 0x00U
  };

  lcd_cgram_define(LCD1602_CHAR_DINO_RUN1, dino_run1);
  lcd_cgram_define(LCD1602_CHAR_DINO_RUN2, dino_run2);
  lcd_cgram_define(LCD1602_CHAR_DINO_JUMP, dino_jump);
  lcd_cgram_define(LCD1602_CHAR_CACTUS, cactus);
  lcd_cgram_define(LCD1602_CHAR_CLOUD, cloud);
}
