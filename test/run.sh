#!/bin/sh
# 在 DGX 本機編譯並執行 test_calc.c（用本機的 cc，不是交叉編譯器）
set -e
cd "$(dirname "$0")"
${CC:-cc} -Wall -Wextra -Werror -o test_calc test_calc.c
./test_calc
