# REDUX UI lab · build del laboratorio (ver docs/PLAN_UI.md)

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.3.1)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2_w CACHE STRING "Board type")

include(pico_sdk_import.cmake)

project(redux_ui_lab C CXX ASM)

pico_sdk_init()

# ---------------------------------------------------------------------------
# LVGL 8.3.11 (lib/lvgl, sin modificar). Configuración en src/ui/lv_conf.h.
# Sus includes son SYSTEM: sus avisos no cuentan para el -Werror del código propio.
# ---------------------------------------------------------------------------
file(GLOB_RECURSE LVGL_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_LIST_DIR}/lib/lvgl/src/*.c)
add_library(lvgl STATIC ${LVGL_SOURCES})
target_compile_definitions(lvgl PUBLIC LV_CONF_INCLUDE_SIMPLE)
target_include_directories(lvgl SYSTEM PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/lib       # "lvgl/lvgl.h" (así lo incluye el export)
    ${CMAKE_CURRENT_LIST_DIR}/src/ui    # "lv_conf.h"
)
target_link_libraries(lvgl PUBLIC pico_stdlib) # Tick de LVGL = reloj del SDK (lv_conf.h)

# ---------------------------------------------------------------------------
# Export de SquareLine: usa su propio CMakeLists.txt (librería "ui").
# ---------------------------------------------------------------------------
add_subdirectory(src/ui/squareline)
target_link_libraries(ui PUBLIC lvgl)

# ---------------------------------------------------------------------------
# Código propio: se compila con avisos estrictos.
# ---------------------------------------------------------------------------
set(REDUX_UI_LAB_SOURCES
    src/main.c
    src/hal/hal_spi.c
    src/hal/hal_keypad.c
    src/hal/hal_keypad_logic.c
    src/hal/hal_wifi.c
    src/ui/st7735.c
    src/ui/lv_port_disp.c
    src/ui/lv_port_indev.c
    src/ui/ui_panel.c
    src/ui/ui_core1.c
    src/lab/lab_lvgl_test.c
    src/lab/lab_keypad_test.c
    src/lab/lab_indev_test.c
    src/lab/lab_screens_test.c
)

add_executable(redux_ui_lab ${REDUX_UI_LAB_SOURCES})

set_source_files_properties(${REDUX_UI_LAB_SOURCES} PROPERTIES
    COMPILE_OPTIONS "-Wall;-Wextra;-Werror;-Wshadow;-Wconversion;-Wdouble-promotion;-Wstrict-prototypes;-Wmissing-prototypes"
)

# src/ es la raíz de includes: "app/ui.h", "hal/board.h", "ui/ui_config.h", "lwipopts.h".
target_include_directories(redux_ui_lab PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)

pico_set_program_name(redux_ui_lab "redux_ui_lab")
pico_set_program_version(redux_ui_lab "0.1")

pico_enable_stdio_uart(redux_ui_lab 0)
pico_enable_stdio_usb(redux_ui_lab 1)

target_link_libraries(redux_ui_lab
    pico_stdlib
    hardware_spi
    hardware_dma
    pico_multicore
    pico_cyw43_arch_lwip_threadsafe_background  # Wi-Fi (hal_wifi, core 0)
    lvgl
    ui
)

pico_add_extra_outputs(redux_ui_lab)
