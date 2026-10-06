/**
  ******************************************************************************
  * @file    calc_page_expr.c
  * @brief   算式页面：输入缓冲 + 长算式的显示窗口 + 键表（含 SHIFT 上档层）。
  *
  *          原来这段代码是 main.c 里的 handle_expr_key（约 150 行）、
  *          input_ends_with_operand、insert_function_template 和两个键表。
  *          搬过来之后 main.c 只剩"把页面返回的动作落到实处"那几行。
  *
  *          依赖：输入缓冲（calc_input）、显示层行工具（calc_view）、键号常量
  *          （calc_keymap）、设置项（FMT 一键切换用）以及 CGRAM 的 ∠ 字符码。
  *          都不碰 HAL，所以能在 PC 上单测。
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "calc_page_expr.h"

#include "calc_keymap.h"
#include "calc_settings.h"
#include "lcd1602.h"

/* Private variables ---------------------------------------------------------*/
static calc_input_t calc_input;
static uint8_t window_start;      /* 窗口左端在算式里的下标 */

/* 每个键往算式里插入的字符，'\0' 表示这个键不插入字符。
   与作者 primary 表一致：键 27 插入 'E'（科学计数法的指数记号）。 */
static const char key_insert_char[TTP229_KEY_COUNT] =
{
  '\0',  /* 00 SHIFT */
  '\0',  /* 01 BACK */
  '\0',  /* 02 MODE */
  '\0',  /* 03 UP */
  '\0',  /* 04 OK */
  '(',   /* 05 */
  ')',   /* 06 */
  '\0',  /* 07 LEFT */
  '\0',  /* 08 DOWN */
  '\0',  /* 09 RIGHT */
  '7', '8', '9',
  '\0',  /* 13 DEL */
  '\0',  /* 14 AC */
  '4', '5', '6', '*', '/',
  '1', '2', '3', '+', '-',
  '0', '.', 'E',
  '\0',  /* 28 FMT */
  '\0'   /* 29 EXE */
};

/* SHIFT 层往算式里插入的记号，取自作者的 shifted 表：
   'p'=π、'd'=∠、'i'、'e'、'l'=log、'n'=ln、'^'=x^y、'q'=√、
   's'=sin、'c'=cos、't'=tan、'A'=Ans。'\0' 表示这个键没有上档。
   其中 c l n p q s t 是函数，插入时会自动补一个左括号。
   Ans 放在 28 号键的上档：键帽上 28 是大字 FMT、小字 ANS，小字就是上档层
   （作者的表把 'A' 放在 27，和键帽丝印不一致，这里照键帽走）。 */
static const char key_insert_shifted[TTP229_KEY_COUNT] =
{
  '\0', '\0', '\0', '\0', '\0',   /* 00-04 无上档 */
  '\0', '\0', '\0', '\0', '\0',   /* 05-09 无上档 */
  'p',  (char)LCD1602_CHAR_ANGLE, 'i',   /* 10-12：π ∠ i；∠ 用 CGRAM 字符码 */
  '\0', '\0',                     /* 13-14 */
  'e',  'l',  'n',  '^',  'q',    /* 15-19：e log ln x^y √ */
  's',  'c',  't',                /* 20-22：sin cos tan */
  '\0', '\0',                     /* 23-24 */
  '\0', '\0',                     /* 25-26 */
  '\0',                           /* 27：键帽上没有上档记号 */
  'A',                            /* 28：ANS —— 插入上一次的结果 */
  '\0'                            /* 29 */
};

/* Private function prototypes -----------------------------------------------*/
static uint8_t input_ends_with_operand(void);
static void insert_function_template(const char *name);
static expr_action_t insert_shifted(char upper);

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  光标左边是否已经是一个完整的操作数。
  * @note   用来决定 x^y 插成 "^()"（底数已输好）还是完整的 "()^()"。
  *         数字、小数点、右括号、常量（pi 的 i、e、Ans 的 A）都算操作数。
  * @retval 1 = 左边是操作数
  */
static uint8_t input_ends_with_operand(void)
{
  char previous;

  if (calc_input.cursor == 0U)
  {
    return 0U;
  }

  previous = calc_input.text[calc_input.cursor - 1U];

  if (((previous >= '0') && (previous <= '9')) || (previous == '.') ||
      (previous == ')') || (previous == 'e') || (previous == 'p') ||
      (previous == 'i') || (previous == 'A'))
  {
    return 1U;
  }

  return 0U;
}

/**
  * @brief  插入一个函数模板："名字()"，并把光标放到括号里面。
  * @note   右括号是模板自带的，所以用户填完参数直接按 EXE 就行；
  *         如果习惯性地再按一次 ')'，只会把光标移过现成的右括号，不会插重复。
  */
static void insert_function_template(const char *name)
{
  while (*name != '\0')
  {
    (void)calc_input_insert(&calc_input, *name);
    name++;
  }

  (void)calc_input_insert(&calc_input, '(');
  (void)calc_input_insert(&calc_input, ')');
  (void)calc_input_move_left(&calc_input);      /* 光标退进括号里 */
}

/**
  * @brief  上档层：把 SHIFT + 某个字符键对应的记号插进算式。
  * @param  upper 键表里的上档记号，调用方已保证不是 '\0'。
  */
static expr_action_t insert_shifted(char upper)
{
  if (upper == 'l')
  {
    /* log 是二元函数：插入 "l(,)"，光标停在第一个参数（底数）位置。
       逗号只占一格，"底数末尾"和"真数开头"本来就是相邻的两格，
       按一下 → 自然就过去了，所以这里不需要额外的跳过标记。 */
    (void)calc_input_insert(&calc_input, 'l');
    (void)calc_input_insert(&calc_input, '(');
    (void)calc_input_insert(&calc_input, ',');
    (void)calc_input_insert(&calc_input, ')');
    (void)calc_input_move_left(&calc_input);
    (void)calc_input_move_left(&calc_input);
  }
  else if (upper == 'n')
  {
    /* 自然对数：插入 ln()，光标停在括号里 */
    insert_function_template("ln");
  }
  else if (upper == 'q')
  {
    /* 平方根：插入 sqrt()，光标停在括号里 */
    insert_function_template("sqrt");
  }
  else if (upper == 'p')
  {
    /* π 写成两个字母 pi，表达式里一眼能看懂 */
    (void)calc_input_insert(&calc_input, 'p');
    (void)calc_input_insert(&calc_input, 'i');
  }
  else if (upper == '^')
  {
    if (input_ends_with_operand() != 0U)
    {
      /* 底数已经输好了：只补 "^()"，光标落在指数位置 */
      (void)calc_input_insert(&calc_input, '^');
      (void)calc_input_insert(&calc_input, '(');
      (void)calc_input_insert(&calc_input, ')');
      (void)calc_input_move_left(&calc_input);
    }
    else
    {
      /* 还没输底数：插完整的 "()^()"，光标落在第一个括号里。
         第一个括号的右括号、^ 和第二个左括号都标成跳过的，
         这样在底数后面按一下 → 就直接到指数位置。 */
      (void)calc_input_insert(&calc_input, '(');
      (void)calc_input_insert(&calc_input, ')');
      (void)calc_input_insert(&calc_input, '^');
      (void)calc_input_insert(&calc_input, '(');
      (void)calc_input_insert(&calc_input, ')');
      calc_input_mark_skip(&calc_input, 1U, 3U);    /* )^( */
      (void)calc_input_move_left(&calc_input);      /* 一次就退到第一个括号里 */
    }
  }
  else
  {
    /* 三角函数：插入 s() / c() / t()，光标停在括号里 */
    if ((upper == 's') || (upper == 'c') || (upper == 't'))
    {
      const char name[2] = { upper, '\0' };

      insert_function_template(name);
    }
    else
    {
      (void)calc_input_insert(&calc_input, upper);
    }
  }

  return EXPR_ACTION_NONE;
}

/* Exported functions --------------------------------------------------------*/

void expr_page_init(void)
{
  calc_input_clear(&calc_input);
  window_start = 0U;
}

calc_input_t *expr_page_input(void)
{
  return &calc_input;
}

void expr_page_window_reset(void)
{
  window_start = 0U;
}

expr_action_t expr_page_handle_key(uint8_t key, uint8_t shifted)
{
  switch (key)
  {
    case TTP229_KEY_BACK:
      /* 作者的设计：BACK 相当于计算器的 CE，清掉整行输入，不是删一个字。 */
      calc_input_clear(&calc_input);
      break;

    case TTP229_KEY_DEL:
      /* 作者的设计：DEL 是退格，删光标左边一个字符。 */
      (void)calc_input_backspace(&calc_input);
      break;

    case TTP229_KEY_AC:
      /* 作者的设计：AC 除了清空输入，还要把界面拉回初始状态（READY）。 */
      calc_input_clear(&calc_input);
      return EXPR_ACTION_CLEAR_RESULT;

    case TTP229_KEY_LEFT:
      (void)calc_input_move_left(&calc_input);
      break;

    case TTP229_KEY_RIGHT:
      (void)calc_input_move_right(&calc_input);
      break;

    case TTP229_KEY_UP:      /* 单行算式：上 = 移到开头 */
      (void)calc_input_move_home(&calc_input);
      break;

    case TTP229_KEY_DOWN:    /* 下 = 移到末尾 */
      (void)calc_input_move_end(&calc_input);
      break;

    case TTP229_KEY_MODE:    /* 作者的设计：MODE 打开模式菜单 */
      return EXPR_ACTION_OPEN_MENU;

    case TTP229_KEY_OK:
      /* OK 按作者的语义留给模式菜单确认，这里不做任何事。 */
      break;

    case TTP229_KEY_EXE:
      return EXPR_ACTION_EVALUATE;

    default:
    {
      /* 上档层：SHIFT 之后按的字符键插入上档记号；上档层没有定义的键不插入。
         动作键（BACK/DEL/AC/方向键…）在上面各自的分支里已经处理完了，
         它们照常执行，只是顺便消费掉上档状态。 */
      const char upper = key_insert_shifted[key];
      const char base = key_insert_char[key];

      if ((shifted != 0U) && (upper != '\0'))
      {
        return insert_shifted(upper);
      }

      if ((shifted == 0U) && (key == TTP229_KEY_FMT))
      {
        /* 无上档的 FMT：在 a+bi 和 r∠θ 之间一键切换（只重画结果，不重算）。 */
        settings_set_value(SETTING_POLAR,
                           (uint8_t)((settings_value(SETTING_POLAR) == 0U) ? 1U : 0U));
        return EXPR_ACTION_REFRESH_RESULT;
      }

      if ((shifted == 0U) && (base != '\0'))
      {
        /* 右括号：右边已经有模板自带的 ')' 时只把光标移过去，不插重复的。 */
        if ((base == ')') && (calc_input.cursor < calc_input.length) &&
            (calc_input.text[calc_input.cursor] == ')'))
        {
          (void)calc_input_move_right(&calc_input);
        }
        else
        {
          (void)calc_input_insert(&calc_input, base);
        }
      }
      break;
    }
  }

  return EXPR_ACTION_NONE;
}

void expr_page_render(char line[VIEW_LINE_WIDTH], uint8_t *cursor_column)
{
  view_format_input(&calc_input, &window_start, line, cursor_column);
}
