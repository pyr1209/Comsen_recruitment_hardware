#ifndef CALC_PAGE_MENU_H
#define CALC_PAGE_MENU_H

#include <stdint.h>

#include "calc_settings.h"
#include "calc_view.h"

/*
 * 菜单页（SELECT MODE）和由它进入的三个设置页（ANGLE UNIT / COMPLEX / POLAR）。
 *
 * 两页放一个模块，是因为它们本来就成对：菜单负责"高亮第几项"，设置页负责
 * "方向键改暂存值"，两边除了导航没有别的状态；值语义（生效/暂存/提交/丢弃）
 * 仍然在 calc_settings.c 里。
 *
 * 页面不自己切页、不自己重算：把"该干什么"当动作返回给调用方（main.c）。
 */

/* 菜单页按键的结果。 */
typedef enum
{
  MENU_ACTION_NONE = 0,        /* 菜单内部处理完了（换高亮项） */
  MENU_ACTION_OPEN_ANGLE,      /* OK 选中的是 ANGLE UNIT */
  MENU_ACTION_OPEN_COMPLEX,    /* OK 选中的是 COMPLEX */
  MENU_ACTION_OPEN_POLAR,      /* OK 选中的是 POLAR */
  MENU_ACTION_OPEN_HISTORY,    /* OK 选中的是 HISTORY */
  MENU_ACTION_OPEN_GAME,       /* OK 选中的是 GAME */
  MENU_ACTION_OPEN_SERIAL,     /* OK 选中的是 SEND FROM PC */
  MENU_ACTION_EXIT_EXPR        /* BACK / MODE：回算式界面 */
} menu_action_t;

/* 设置页按键的结果。 */
typedef enum
{
  OPTION_ACTION_NONE = 0,      /* 设置页内部处理完了（改暂存值） */
  OPTION_ACTION_APPLY_EXIT,    /* OK 且值真的变了：请重算一次然后回算式界面 */
  OPTION_ACTION_EXIT_EXPR,     /* OK 且值没变 / MODE：已丢弃，直接回算式界面 */
  OPTION_ACTION_EXIT_MENU      /* BACK：已丢弃，回菜单 */
} option_action_t;

/* 上电：高亮回到第一项。 */
void menu_page_init(void);

/* 菜单页按键：方向键换项，OK 选中，BACK / MODE 退出。 */
menu_action_t menu_page_handle_key(uint8_t key);

/* 画菜单页的两行。 */
void menu_page_render(char top[VIEW_LINE_WIDTH], char bottom[VIEW_LINE_WIDTH]);

/* 设置页按键：方向键只改暂存值，OK 提交，BACK / MODE 丢弃。 */
option_action_t option_page_handle_key(uint8_t key, setting_id_t id);

/* 画设置页的两行（标题 + DEG/RAD 这种两选项行）。 */
void option_page_render(setting_id_t id, char top[VIEW_LINE_WIDTH],
                        char bottom[VIEW_LINE_WIDTH]);

#endif
