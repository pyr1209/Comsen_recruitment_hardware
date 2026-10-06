#!/bin/sh
#
# 在 PC 上跑这些单元测试：不需要板子、不需要交叉编译器，只要一个 cc。
#
#   cd example/tests && ./run_all.sh
#
# 每个测试都直接编译 ../Core/Src 里的**真实源码**（不是复制一份逻辑），
# 所以既能验证行为，也能保证"改了代码没改测试"这种偷懒混不过去。
# 固件构建（CMakeLists.txt）显式列了参与编译的文件，tests/ 不参与。
#

set -e

CC=${CC:-cc}
CFLAGS="-std=c11 -Wall -Wextra -I../Core/Inc"
SRC=../Core/Src
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT

failed=0
total=0

# 名字:需要的源文件
run_test()
{
  name=$1
  shift
  total=$((total + 1))

  if ! $CC $CFLAGS -o "$OUT/$name" "$name.c" "$@" -lm 2>"$OUT/$name.log"; then
    echo "编译失败: $name"
    sed -n '1,20p' "$OUT/$name.log"
    failed=$((failed + 1))
    return
  fi

  if "$OUT/$name" >"$OUT/$name.out" 2>&1; then
    printf 'PASS  %-16s (%s 项断言)\n' "$name" \
           "$(grep -c 'ok' "$OUT/$name.out" || true)"
  else
    printf 'FAIL  %-16s\n' "$name"
    grep -E '^(FAIL|.*失败)' "$OUT/$name.out" | head -20
    failed=$((failed + 1))
  fi
}

run_test view_test        "$SRC/calc_view.c" "$SRC/calc_input.c"
run_test settings_test    "$SRC/calc_settings.c"
run_test ui_test          "$SRC/calc_ui.c"
run_test serial_rx_test   "$SRC/calc_serial_rx.c" "$SRC/calc_view.c"
run_test game_test        "$SRC/dino_game.c"
run_test expr_test        "$SRC/calc_page_expr.c" "$SRC/calc_input.c" \
                          "$SRC/calc_view.c" "$SRC/calc_settings.c"
run_test menu_test        "$SRC/calc_page_menu.c" "$SRC/calc_settings.c" \
                          "$SRC/calc_view.c"
run_test result_test      "$SRC/calc_result.c" "$SRC/calc_history.c" \
                          "$SRC/calc_input.c" "$SRC/calc_format.c" \
                          "$SRC/calc_settings.c" "$SRC/calc_view.c" \
                          "$SRC/calculator_engine.c"
run_test history_test     "$SRC/calc_page_history.c" "$SRC/calc_history.c" \
                          "$SRC/calc_input.c" "$SRC/calc_format.c" \
                          "$SRC/calc_result.c" "$SRC/calc_settings.c" \
                          "$SRC/calc_ui.c" "$SRC/calc_view.c" \
                          "$SRC/calculator_engine.c"

echo
if [ "$failed" -ne 0 ]; then
  echo "$failed / $total 个测试失败"
  exit 1
fi
echo "$total 个测试全部通过"
