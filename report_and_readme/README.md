# STM32 触摸计算器 · 工程说明

## 一、这是什么

一块基于 **STM32F103C8T6** 的触摸计算器

- 支持四则、括号、幂、科学计数法、三角函数、对数、开方、复数、极坐标运算；
- 支持复数运算与极坐标输入输出（`a+bi` ↔ `r∠θ`）；
- 有角度单位（DEG/RAD）、数域（实数/复数）、是否输出为极坐标形式三个全局设置，以及历史记录、串口显示、小恐龙小游戏三个界面；
- 不使用 RTOS：初始化后在 `while (1)` 里按固定周期轮流处理按键、串口和刷屏。

当前 Release 占用：**FLASH 41124 B / 64 KB（62.8%），RAM 6608 B / 20 KB（32.3%）**。

## 二、目录与模块

### 自己实现的模块（`example/Core/`）

| 文件 | 行数 | 职责 |
| --- | --- | --- |
| `Src/main.c` | 1984 | 裸机主循环、界面状态机、按键映射、结果展示 |
| `Src/calculator_engine.c` | 814 | 表达式解析（递归下降）与求值，内部统一按复数运算 |
| `Src/calc_format.c` | 577 | 结果文本格式化（不依赖 printf 浮点） |
| `Src/dino_game.c` | 321 | 小恐龙跳仙人掌：滚动、跳跃、碰撞、计分、昼夜 |
| `Src/lcd_cgram.c` | 241 | 1602 的 CGRAM 自定义字符（∠ 和游戏精灵） |
| `Src/touch_model.c` | 202 | 电极串扰的组合识别（位图 → 键号） |
| `Src/calc_input.c` | 180 | 算式输入缓冲：内容、光标、模板跳过标记 |
| `Src/ttp229.c` | 152 | TTP229 两线时序驱动 |
| `Src/touch_filter.c` | 147 | 按键去抖与边沿判定 |
| `Src/calc_history.c` | 68 | 历史记录环形缓冲 |

合计 10 个文件、约 4700 行。

### 用到的作者提供内容

| 内容 | 说明 |
| --- | --- |
| `example/lib/liblcd1602.a` | 1602 驱动本体（`lcd1602_init` / `lcd1602_write_frame`），仍然链接使用 |
| CubeMX 生成的工程骨架 | `Drivers/`（HAL + CMSIS）、`USB_DEVICE/`、`cmake/`、启动文件与链接脚本 |
| USB 中间件 | `Middlewares/ST/STM32_USB_Device_Library`（CDC 类） |
| 公开头文件 | `lcd1602.h`、`ttp229.h`、`touch_*.h`、`calculator_engine.h` 等，保持同名同接口 |

被替换掉的 5 个静态库（原库已从 `lib/` 移除）：

| 原库 | 现在的实现 |
| --- | --- |
| `libttp229.a` | `Core/Src/ttp229.c` |
| `libtouch_filter.a` | `Core/Src/touch_filter.c` |
| `libtouch_model.a` | `Core/Src/touch_model.c` |
| `libcalculator_engine.a` | `Core/Src/calculator_engine.c` |
| `libcalculator_app.a` | `Core/Src/main.c`（裸机主循环，不用 FreeRTOS） |

## 三、功能一览

### 计算能力

| 类别 | 内容 |
| --- | --- |
| 基本 | 四则、括号（任意嵌套）、一元正负号、幂（右结合）、`×10^x` 的 E 记数法 |
| 函数 | `sin` `cos` `tan`、`ln`、`log`（任意底，两参数）、`√`、`x^y` |
| 常数与引用 | `π`、`e`、`Ans`（上一次结果） |
| 复数 | `i`、复数的四则/幂/开方/对数/指数/三角 |
| 极坐标 | 输入 `a∠b`；输出可写成 `r∠θ` |
| 角度单位 | DEG / RAD 全局生效（三角函数入参、算出的角度、极坐标角度） |
| 错误提示 | `Syntax ERROR` / `Math ERROR` / `Div0 ERROR` 三类分开提示 |

### 界面与按键

`MODE` 菜单共 6 项：

| 菜单项 | 内容 |
| --- | --- |
| ANGLE UNIT | DEG / RAD |
| COMPLEX | COMP（只算实数）/ CMPLX（允许复数） |
| POLAR | RECT（`a+bi`）/ POLAR（`r∠θ`） |
| HISTORY | 最近 8 条算式与结果，可翻页、可把算式装回输入行 |
| GAME | 小恐龙跳仙人掌（计分、渐进加速、每 10 秒昼夜交替、仙人掌有高矮和 1~3 连） |
| SEND FROM PC | 显示电脑经虚拟串口发来的内容 |

设置项都是"暂存值 + OK 提交 / BACK 丢弃"的语义，提交后直接回结果界面。

按键：30 个触摸键全部有功能；`SHIFT` 上档提供 `π ∠ i e log ln x^y √ sin cos tan Ans`；
`FMT` 一键在 `a+bi` 与 `r∠θ` 之间切换；
`ANS` 把上一次的结果插进当前算式。游戏里 `↑` / `→` 跳跃，撞到后按 `OK` 重开。

## 四、已知限制


- 按 EXE 时不会自动补右括号：`s(30` 会报 Syntax ERROR（函数模板自带括号，正常输入不受影响）。
- 不允许隐式乘法：`2i` 必须写成 `2*i`。
- 数值精度受 `float` 限制：约 7 位有效数字，很大的整数会用科学计数法表示，不保证逐位精确。
- 显示只有 16×2 字符；复数过长时会自动改用科学计数法。
- 小游戏的节奏参数（跳跃 6 格、仙人掌间隔 8~12 格、昼夜 10 秒）是按手感和测试调的，真机上仍可再微调。




