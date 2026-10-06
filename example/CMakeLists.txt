cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME software)

# Include toolchain file
include("cmake/gcc-arm-none-eabi.cmake")

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME} LANGUAGES C ASM)
message("Build type: " ${CMAKE_BUILD_TYPE})

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # 自己实现的 TTP229 驱动（取代作者提供的 libttp229.a，原库已删除）。
    Core/Src/ttp229.c
    # 自己实现的按键去抖与事件处理，取代 lib/libtouch_filter.a。
    Core/Src/touch_filter.c
    # 自己实现的算式输入缓冲（内容 + 长度 + 光标）。
    Core/Src/calc_input.c
    # 自己实现的触摸组合识别，取代 lib/libtouch_model.a。
    Core/Src/touch_model.c
    # 自己实现的表达式求值，取代 lib/libcalculator_engine.a。
    Core/Src/calculator_engine.c
    # 自己实现的结果文本格式化（不引入 printf 浮点格式化）。
    Core/Src/calc_format.c
    # 自己实现的历史记录环形缓冲（存算式和结果，供 HISTORY 页面翻看）。
    Core/Src/calc_history.c
    # 自己实现的显示层行工具（行填充/写文本/算式窗口），无状态、可在 PC 上单测。
    Core/Src/calc_view.c
    # 自己实现的全局设置项（角度单位/数域/结果形式/极坐标原点 + 提交语义）。
    Core/Src/calc_settings.c
    # 自己实现的界面状态机（当前页面 + 唯一的切页入口）。
    Core/Src/calc_ui.c
    # 自己实现的小游戏逻辑（小恐龙跳仙人掌，纯状态机，不碰硬件）。
    Core/Src/dino_game.c
    # 给 1602 定义自定义字符（∠），作者库没暴露 CGRAM 接口。
    Core/Src/lcd_cgram.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# The calculator plus retained Cube USB sources no longer fits 64 KiB at -O0.
# -Og preserves source-level debugging while keeping the Debug image usable.
target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE
    $<$<CONFIG:Debug>:-Og>
)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/liblcd1602.a"
    # libttp229.a 已由自己实现的 Core/Src/ttp229.c 取代，不再链接；
    # libtouch_filter.a 已由自己实现的 Core/Src/touch_filter.c 取代；
    # libtouch_model.a 已由自己实现的 Core/Src/touch_model.c 取代；
    # libcalculator_engine.a 已由自己实现的 Core/Src/calculator_engine.c 取代；
    # libcalculator_app.a 已由自己实现的 Core/Src/main.c 取代（裸机主循环，不用 FreeRTOS）。
    # 上面这些原库已从 lib/ 删除，只保留仍需使用的 liblcd1602.a。

    # Add user defined libraries
)

# POST_BUILD 在目标输出目录中执行，因此这里只用相对文件名：
# 绝对路径中的空格和括号无法被 CMake 完整转义，会让 /bin/sh 解析失败。
add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex
            $<TARGET_FILE_NAME:${CMAKE_PROJECT_NAME}>
            ${CMAKE_PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary
            $<TARGET_FILE_NAME:${CMAKE_PROJECT_NAME}>
            ${CMAKE_PROJECT_NAME}.bin
    COMMENT "Generating HEX and BIN firmware images"
)
