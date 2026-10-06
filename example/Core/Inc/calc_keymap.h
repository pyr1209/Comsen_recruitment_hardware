#ifndef CALC_KEYMAP_H
#define CALC_KEYMAP_H

/*
 * 30 个触摸键的"键号"（T0..T29），以及其中动作键的编号。
 *
 * 键号是硬件层（ttp229 + touch_filter + touch_model）识别出来的结果，
 * 被界面层各个页面共用，所以单独放一个只有常量的头文件里。
 */

#define TTP229_KEY_COUNT      30U

/* 功能键的键号，其余键号是字符键。 */
#define TTP229_KEY_SHIFT      0U
#define TTP229_KEY_BACK       1U
#define TTP229_KEY_MODE       2U
#define TTP229_KEY_UP         3U
#define TTP229_KEY_OK         4U
#define TTP229_KEY_LEFT       7U
#define TTP229_KEY_DOWN       8U
#define TTP229_KEY_RIGHT      9U
#define TTP229_KEY_DEL        13U
#define TTP229_KEY_AC         14U
#define TTP229_KEY_FMT        28U
#define TTP229_KEY_EXE        29U

#endif
