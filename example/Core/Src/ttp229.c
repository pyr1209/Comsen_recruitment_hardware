/**
  ******************************************************************************
  * @file    ttp229.c
  * @brief   自己实现的 TTP229 两线读取。
  *
  *          对外接口与作者提供的 libttp229.a 完全同名（ttp229_read_physical），
  *          因此链接时本文件优先，静态库里同名成员不会被拉进来，两者不会冲突。
  *
  *          硬件连接（由作者库反汇编确认）：
  *            A 片 SCL = PB6，SDO = PB7
  *            B 片 SCL = PB8，SDO = PB9
  *          读取时序：SCL 拉低 -> 等 5 µs -> 采样 SDO（低电平表示该通道被触发）
  *                    -> SCL 拉高 -> 等 5 µs，共 16 个通道。
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "ttp229.h"

#include "main.h"

/* Private define ------------------------------------------------------------*/
/* 引脚定义：TTP229 是普通 GPIO 时序，不使用 I2C 外设。 */
#define TTP229_PORT                GPIOB
#define TTP229_SCL_A_PIN           GPIO_PIN_6
#define TTP229_SDO_A_PIN           GPIO_PIN_7
#define TTP229_SCL_B_PIN           GPIO_PIN_8
#define TTP229_SDO_B_PIN           GPIO_PIN_9

#define TTP229_CHANNELS_PER_CHIP   16U
#define TTP229_SAMPLE_DELAY_US     5U
#define TTP229_UNUSED_CHANNEL      0xFFU
#define TTP229_KEY_COUNT           30U

/* Private variables ---------------------------------------------------------*/
/* TIM3 已在 main.c 中定义为 1 MHz 的自由运行计数器，这里只读取它的 CNT。 */
extern TIM_HandleTypeDef htim3;

/* 通道号到逻辑键号（bit0..bit29 对应 T0..T29）的固定接线映射，
   数值取自作者库的只读表，两片各有一个未使用通道。 */
static const uint8_t ttp229_channel_a_to_key[TTP229_CHANNELS_PER_CHIP] =
{
  7U, 11U, 6U, 1U, 0U, 5U, 10U, 2U,
  TTP229_UNUSED_CHANNEL,
  14U, 9U, 4U, 3U, 8U, 13U, 12U
};

static const uint8_t ttp229_channel_b_to_key[TTP229_CHANNELS_PER_CHIP] =
{
  27U, 15U, 20U, 25U, 16U, 21U, 26U, 17U,
  22U, 28U, 23U, 18U, 29U, 24U, 19U,
  TTP229_UNUSED_CHANNEL
};

/* Private function prototypes -----------------------------------------------*/
static void ttp229_delay_us(uint16_t microseconds);
static uint16_t ttp229_read_channels(uint16_t scl_pin, uint16_t sdo_pin);
static uint32_t ttp229_map_channels(uint16_t channels_a, uint16_t channels_b);

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  微秒级忙等。TIM3 以 1 MHz 计数，1 个计数为 1 µs。
  * @note   使用 16 位无符号减法，计数器回绕时不会死等。
  */
static void ttp229_delay_us(uint16_t microseconds)
{
  const uint16_t start = (uint16_t)__HAL_TIM_GET_COUNTER(&htim3);

  while ((uint16_t)((uint16_t)__HAL_TIM_GET_COUNTER(&htim3) - start) < microseconds)
  {
  }
}

/**
  * @brief  读一颗 TTP229 的 16 个通道。
  * @param  scl_pin 该片的时钟脚
  * @param  sdo_pin 该片的数据脚
  * @retval bit i 为 1 表示第 i 个通道被触发
  */
static uint16_t ttp229_read_channels(uint16_t scl_pin, uint16_t sdo_pin)
{
  uint16_t channels = 0U;
  uint8_t index;

  for (index = 0U; index < TTP229_CHANNELS_PER_CHIP; ++index)
  {
    /* 拉低时钟，等数据稳定后在低电平中段采样。 */
    HAL_GPIO_WritePin(TTP229_PORT, scl_pin, GPIO_PIN_RESET);
    ttp229_delay_us(TTP229_SAMPLE_DELAY_US);

    if (HAL_GPIO_ReadPin(TTP229_PORT, sdo_pin) == GPIO_PIN_RESET)
    {
      channels |= (uint16_t)(1U << index);
    }

    /* 拉高时钟，准备下一个通道。 */
    HAL_GPIO_WritePin(TTP229_PORT, scl_pin, GPIO_PIN_SET);
    ttp229_delay_us(TTP229_SAMPLE_DELAY_US);
  }

  return channels;
}

/**
  * @brief  把两片的通道位按接线表折算成 30 位键位图。
  */
static uint32_t ttp229_map_channels(uint16_t channels_a, uint16_t channels_b)
{
  uint32_t key_bitmap = 0U;
  uint8_t index;

  for (index = 0U; index < TTP229_CHANNELS_PER_CHIP; ++index)
  {
    if ((channels_a & (uint16_t)(1U << index)) != 0U)
    {
      const uint8_t key = ttp229_channel_a_to_key[index];

      if (key < TTP229_KEY_COUNT)
      {
        key_bitmap |= (1UL << key);
      }
    }

    if ((channels_b & (uint16_t)(1U << index)) != 0U)
    {
      const uint8_t key = ttp229_channel_b_to_key[index];

      if (key < TTP229_KEY_COUNT)
      {
        key_bitmap |= (1UL << key);
      }
    }
  }

  return key_bitmap;
}

/* Exported functions --------------------------------------------------------*/

/**
  * @brief  读取 30 个物理按键的当前状态。
  * @note   约定每 10 ms 调用一次；去抖由 touch_filter 负责。
  * @retval bit0..bit29 对应物理按键 T0..T29，1 表示按下
  */
uint32_t ttp229_read_physical(void)
{
  const uint16_t channels_a = ttp229_read_channels(TTP229_SCL_A_PIN, TTP229_SDO_A_PIN);
  const uint16_t channels_b = ttp229_read_channels(TTP229_SCL_B_PIN, TTP229_SDO_B_PIN);

  return ttp229_map_channels(channels_a, channels_b);
}
