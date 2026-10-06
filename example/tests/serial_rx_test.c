/*
 * 串口接收的行缓冲规则：直接编译仓库里真实的 calc_serial_rx.c（不依赖 HAL）。
 * 覆盖：占位内容、累计字符、退格、换行后保留上一行、CRLF、满行左移、控制字符忽略。
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "calc_serial_rx.h"

static unsigned failures;

static void feed(const char *text)
{
  while (*text != '\0')
  {
    serial_rx_feed((uint8_t)*text);
    text++;
  }
}

static void expect_line(const char *label, const char *wanted)
{
  char shown[17];
  int8_t end;

  memcpy(shown, serial_rx_text(), 16U);
  shown[16] = '\0';
  for (end = 15; end >= 0; --end)
  {
    if (shown[end] != ' ') break;
    shown[end] = '\0';
  }

  if (strcmp(shown, wanted) != 0)
  {
    printf("  FAIL %-42s 实际=\"%s\" 期望=\"%s\"\n", label, shown, wanted);
    failures++;
  }
  else
  {
    printf("  ok   %-42s = \"%s\"\n", label, shown);
  }
}

int main(void)
{
  puts("场景 1：初始化（占位内容 / 全空格）");
  serial_rx_init("SEND FROM PC");
  expect_line("带占位内容", "SEND FROM PC");
  serial_rx_init(NULL);
  expect_line("NULL → 全空格", "");

  puts("场景 2：累计字符");
  serial_rx_init(NULL);
  feed("HELLO");
  expect_line("收到 HELLO", "HELLO");

  puts("场景 3：退格");
  serial_rx_feed((uint8_t)'\b');
  expect_line("退一格删掉 O", "HELL");
  serial_rx_init(NULL);
  serial_rx_feed((uint8_t)'\b');
  expect_line("空行退格不会出错", "");

  puts("场景 4：换行后上一行留在屏幕上，下一批字符才清行");
  serial_rx_init(NULL);
  feed("AB\r");
  expect_line("回车后仍显示 AB", "AB");
  feed("CD");
  expect_line("新一行从清屏开始", "CD");

  puts("场景 5：CRLF 连着发（效果 = 一次换行）");
  serial_rx_init(NULL);
  feed("XY\r\nZW");
  expect_line("CRLF 之后显示 ZW", "ZW");

  puts("场景 6：一行满 16 格 → 整行左移，显示最新内容");
  serial_rx_init(NULL);
  feed("12345678901234567890");   /* 20 个字符 */
  expect_line("只剩最后 16 格", "5678901234567890");

  puts("场景 7：控制字符忽略（0x01 / 0x0B）");
  serial_rx_init(NULL);
  feed("OK");
  serial_rx_feed(0x01U);
  serial_rx_feed(0x0BU);
  feed("!");
  expect_line("控制字符不占格", "OK!");

  printf("\n%s（失败 %u 处）\n", failures == 0U ? "PASS" : "FAIL", failures);
  return failures == 0U ? 0 : 1;
}
