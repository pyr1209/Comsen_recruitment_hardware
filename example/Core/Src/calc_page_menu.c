/**
  ******************************************************************************
  * @file    calc_page_menu.c
  * @brief   菜单页（SELECT MODE）+ 三个设置页（ANGLE UNIT / COMPLEX / POLAR）。
  *
  *          原来这段代码是 main.c 里的 handle_menu_key、handle_option_page、
  *          menu_item_names、menu_index 和 render 里的 4 个分支。
  *
  *          页面只做"按键 → 动作"和"状态 → 两行文本"，切页与重算交给调用方。
  *          值语义（生效/暂存/提交/丢弃）在 calc_settings.c 里，本文件只是它的
  *          界面：哪个键对应什么操作、按完留在哪一页。
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "calc_page_menu.h"

#include "calc_keymap.h"

/* Private variables ---------------------------------------------------------*/
static const char *const menu_item_names[] =
{
  "ANGLE UNIT",
  "COMPLEX",
  "POLAR",
  "HISTORY",
  "GAME",
  "SEND FROM PC"
};

#define MENU_ITEM_COUNT ((uint8_t)(sizeof(menu_item_names) / sizeof(menu_item_names[0])))

static uint8_t menu_index;        /* 菜单里高亮的项 */

/* 三个设置页的标题和两个选项。顺序与 setting_id_t 一致。 */
static const char *const option_titles[SETTING_COUNT]  = { "ANGLE UNIT", "COMPLEX", "POLAR" };
static const char *const option_firsts[SETTING_COUNT]  = { "DEG",        "COMP",    "RECT" };
static const char *const option_seconds[SETTING_COUNT] = { "RAD",        "CMPLX",   "POLAR" };

/* Exported functions --------------------------------------------------------*/

void menu_page_init(void)
{
  menu_index = 0U;
}

menu_action_t menu_page_handle_key(uint8_t key)
{
  switch (key)
  {
    case TTP229_KEY_UP:
    case TTP229_KEY_LEFT:
      menu_index = (menu_index == 0U) ? (uint8_t)(MENU_ITEM_COUNT - 1U)
                                      : (uint8_t)(menu_index - 1U);
      break;

    case TTP229_KEY_DOWN:
    case TTP229_KEY_RIGHT:
      menu_index = (uint8_t)((menu_index + 1U) % MENU_ITEM_COUNT);
      break;

    case TTP229_KEY_OK:
      switch (menu_index)
      {
        case 0U:  return MENU_ACTION_OPEN_ANGLE;
        case 1U:  return MENU_ACTION_OPEN_COMPLEX;
        case 2U:  return MENU_ACTION_OPEN_POLAR;
        case 3U:  return MENU_ACTION_OPEN_HISTORY;
        case 4U:  return MENU_ACTION_OPEN_GAME;
        case 5U:  return MENU_ACTION_OPEN_SERIAL;
        default:  break;
      }
      break;

    case TTP229_KEY_BACK:
    case TTP229_KEY_MODE:
      return MENU_ACTION_EXIT_EXPR;

    default:
      break;
  }

  return MENU_ACTION_NONE;
}

void menu_page_render(char top[VIEW_LINE_WIDTH], char bottom[VIEW_LINE_WIDTH])
{
  const char *name = menu_item_names[menu_index];
  uint8_t position;

  view_set_text(top, "SELECT MODE");

  /* 当前项前面加 '>'，和作者菜单的标记方式一致。 */
  view_fill(bottom, ' ');
  bottom[0] = '>';
  for (position = 0U; (position < (VIEW_LINE_WIDTH - 2U)) &&
                      (name[position] != '\0'); ++position)
  {
    bottom[2U + position] = name[position];
  }
}

option_action_t option_page_handle_key(uint8_t key, setting_id_t id)
{
  switch (key)
  {
    case TTP229_KEY_UP:
    case TTP229_KEY_LEFT:
      settings_select(id, 0U);             /* 只改暂存值 */
      break;

    case TTP229_KEY_DOWN:
    case TTP229_KEY_RIGHT:
      settings_select(id, 1U);
      break;

    case TTP229_KEY_OK:
      /* 提交；值真的变了才要求重算一次（和原来的 if 判断一致）。 */
      return (settings_commit(id) != 0U) ? OPTION_ACTION_APPLY_EXIT
                                         : OPTION_ACTION_EXIT_EXPR;

    case TTP229_KEY_BACK:
      settings_discard(id);                /* 丢弃：暂存恢复成生效值 */
      return OPTION_ACTION_EXIT_MENU;

    case TTP229_KEY_MODE:
      settings_discard(id);                /* 直接退出也要丢弃 */
      return OPTION_ACTION_EXIT_EXPR;

    default:
      break;
  }

  return OPTION_ACTION_NONE;
}

void option_page_render(setting_id_t id, char top[VIEW_LINE_WIDTH],
                        char bottom[VIEW_LINE_WIDTH])
{
  view_set_text(top, option_titles[id]);
  /* 名字在第 1 / 11 列，前面一格放标记（'>' 正在选、'*' 已生效）。 */
  view_option_line(bottom, option_firsts[id], 1U, option_seconds[id], 11U,
                   settings_pending(id), settings_value(id));
}
