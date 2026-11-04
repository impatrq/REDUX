# REDUX UI lab · pruebas en PC (gcc del sistema, no el de la Pico)
#
# Desde la raíz del laboratorio:
#   cmake -S tests -B build-tests -G Ninja -DCMAKE_C_COMPILER=gcc
#   cmake --build build-tests
#   ctest --test-dir build-tests --output-on-failure

cmake_minimum_required(VERSION 3.13)
project(redux_ui_lab_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

enable_testing()

set(REDUX_SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/../src)
set(REDUX_TEST_WARNINGS
    -Wall -Wextra -Werror -Wshadow -Wconversion -Wdouble-promotion
    -Wstrict-prototypes -Wmissing-prototypes
)

add_executable(test_keypad
    test_keypad.c
    ${REDUX_SRC_DIR}/hal/hal_keypad_logic.c
)
target_include_directories(test_keypad PRIVATE ${REDUX_SRC_DIR})
target_compile_options(test_keypad PRIVATE ${REDUX_TEST_WARNINGS})
add_test(NAME test_keypad COMMAND test_keypad)
