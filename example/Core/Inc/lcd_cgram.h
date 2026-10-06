#ifndef LCD_CGRAM_H
#define LCD_CGRAM_H

#include <stdint.h>

/*
 * 给 1602 定义自定义字符。
 *
 * 作者的 liblcd1602.a 只提供整屏写文本，没有暴露写 CGRAM 的接口，
 * 所以这里按它自己的引脚和时序直接写几个字节——引脚和协议都是从
 * 那个库反汇编出来的，和 MX_GPIO_Init 的配置一致：
 *   RS = PB10，E = PB11，D4-D7 = PB12-PB15
 * 调用前必须先 lcd1602_init()（引脚方向由 CubeMX 的 GPIO 初始化负责）。
 */

/* 定义一个 5x8 的自定义字符到 CGRAM 的第 slot 格（0-7）。 */
void lcd_cgram_define(uint8_t slot, const uint8_t pattern[8]);

/* 把 LCD1602_CHAR_ANGLE 这一格定义成 ∠，之后在文本里写这个字符码就会显示 ∠。 */
void lcd_cgram_define_angle(void);

/*
 * 小游戏用的精灵。CGRAM 一共 8 格，安排如下（槽 1 的 ∠ 是算式用的）：
 *   0  天空装饰：白天是云、夜里是星星——两者共用这一格，运行时换点阵
 *      （见 lcd_cgram_define_sky），这样才腾得出格子给高仙人掌
 *   2  恐龙跑动第 1 帧      3  恐龙跑动第 2 帧      4  恐龙腾空
 *   5  矮仙人掌             6  高仙人掌             7  月亮
 * 每个精灵都是 5x8 点阵：8 个字节分别对应从上到下的 8 行，
 * 每行的低 5 位是 5 个像素，bit0 是最右边那一列。
 */
#define LCD1602_CHAR_SKY         0U    /* 云 / 星星（白天黑夜共用，点阵可切） */
#define LCD1602_CHAR_DINO_RUN1   2U    /* 恐龙跑动第 1 帧 */
#define LCD1602_CHAR_DINO_RUN2   3U    /* 恐龙跑动第 2 帧（收腿） */
#define LCD1602_CHAR_DINO_JUMP   4U    /* 恐龙腾空（身体提起、腿收起） */
#define LCD1602_CHAR_CACTUS      5U    /* 矮仙人掌 */
#define LCD1602_CHAR_CACTUS_TALL 6U    /* 高仙人掌 */
#define LCD1602_CHAR_MOON        7U    /* 月亮（天黑以后固定在第 1 行最左边） */

void lcd_cgram_define_game_sprites(void);

/* 切换天空装饰的点阵：night = 0 换成云，night = 1 换成星星。 */
void lcd_cgram_define_sky(uint8_t night);

#endif
