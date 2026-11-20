# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

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

# Compilar también los programas de banco (Pruebas/banco/*). ON por defecto; apagar con
# -DREDUX_BUILD_BANCOS=OFF para construir solo el firmware principal, sin las pruebas.
option(REDUX_BUILD_BANCOS "Compilar los programas de banco (Pruebas/banco)" ON)

# Fuente de la señal EMG por canal, elegible al compilar sin editar config.h. Valores: ADC
# o SERIAL (default SERIAL en ambos, como el banco). Para la prueba con el bíceps real sin
# sensor de tríceps: -DREDUX_EMG_BIC=ADC -DREDUX_EMG_TRI=SERIAL. Se traducen a los macros
# EMG_SOURCE_BIC / EMG_SOURCE_TRI (ver src/common/config.h).
set(REDUX_EMG_BIC "SERIAL" CACHE STRING "Fuente EMG del biceps: ADC o SERIAL")
set(REDUX_EMG_TRI "SERIAL" CACHE STRING "Fuente EMG del triceps: ADC o SERIAL")
set_property(CACHE REDUX_EMG_BIC PROPERTY STRINGS ADC SERIAL)
set_property(CACHE REDUX_EMG_TRI PROPERTY STRINGS ADC SERIAL)
foreach(emg_opt REDUX_EMG_BIC REDUX_EMG_TRI)
        if(NOT (${${emg_opt}} STREQUAL "ADC" OR ${${emg_opt}} STREQUAL "SERIAL"))
                message(FATAL_ERROR "${emg_opt} debe ser ADC o SERIAL (es '${${emg_opt}}')")
        endif()
endforeach()

# Interfaz de usuario del firmware principal. PANEL = pantalla ST7735 + teclado 4x4 con LVGL
# (toma las fuentes de ../UI y corre la UI en el core 1); BASIC = la UI por serie actual
# (ui_basic). Por defecto PANEL. Los programas de banco usan siempre ui_basic.
set(REDUX_UI PANEL CACHE STRING "Interfaz: PANEL (pantalla+teclado+LVGL) o BASIC (serie)")
set_property(CACHE REDUX_UI PROPERTY STRINGS PANEL BASIC)
if(NOT (REDUX_UI STREQUAL "PANEL" OR REDUX_UI STREQUAL "BASIC"))
        message(FATAL_ERROR "REDUX_UI debe ser PANEL o BASIC (es '${REDUX_UI}')")
endif()

# Fuente del ángulo del encoder. AS5600 = encoder real por I2C; SIM = encoder simulado que
# integra el duty aplicado (sensors/enc_sim), para probar sin hardware. Por defecto AS5600:
# SIM nunca queda por defecto, hay que pedirlo con -DREDUX_ENC=SIM.
set(REDUX_ENC AS5600 CACHE STRING "Encoder: AS5600 (real por I2C) o SIM (simulado)")
set_property(CACHE REDUX_ENC PROPERTY STRINGS AS5600 SIM)
if(NOT (REDUX_ENC STREQUAL "AS5600" OR REDUX_ENC STREQUAL "SIM"))
        message(FATAL_ERROR "REDUX_ENC debe ser AS5600 o SIM (es '${REDUX_ENC}')")
endif()

# Raíz de la línea UI (mismo repo, carpeta hermana). De acá salen LVGL, el export de
# SquareLine y las fuentes de pantalla/teclado/Wi-Fi cuando REDUX_UI=PANEL.
set(REDUX_UI_DIR ${CMAKE_CURRENT_LIST_DIR}/../UI)

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(redux-firmware C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Capa de abstracción de hardware (hal/). Única capa que toca el SDK junto a los
# main.c de cada ejecutable. Se comparte entre el firmware y los programas de prueba.
set(REDUX_HAL_SOURCES
        src/hal/hal_time.c
        src/hal/hal_adc.c
        src/hal/hal_pwm.c
        src/hal/hal_i2c.c
        src/hal/hal_gpio.c
        src/hal/hal_button.c
        src/hal/hal_wdt.c
        src/hal/hal_rand.c
)

# Drivers de actuadores (actuators/). C portable: usan solo el HAL, nunca el SDK.
set(REDUX_ACTUATOR_SOURCES
        src/actuators/bts7960.c
)

# Drivers de sensores (sensors/). C portable: usan solo el HAL, nunca el SDK.
# Comunes a todas las builds (el encoder va aparte, según REDUX_ENC).
set(REDUX_SENSOR_SOURCES_COMMON
        src/sensors/rom_calib.c
        src/sensors/emg.c
        src/sensors/emg_calib.c
        src/sensors/emg_source.c
)
# Fuente del encoder para el firmware principal: as5600.c (real) o enc_sim.c (simulado). Las
# dos implementan la misma API (as5600.h), así que solo se compila una. Los bancos usan
# siempre el AS5600 real (son herramientas de hardware), ver REDUX_SENSOR_SOURCES.
if(REDUX_ENC STREQUAL "SIM")
        set(REDUX_ENC_SOURCE src/sensors/enc_sim.c)
else()
        set(REDUX_ENC_SOURCE src/sensors/as5600.c)
endif()
# Lista completa de sensores con el AS5600 real (bancos y lista de flags estrictas).
set(REDUX_SENSOR_SOURCES
        src/sensors/as5600.c
        ${REDUX_SENSOR_SOURCES_COMMON}
)

# Interfaz de usuario (app/). ui_basic implementa el enganche fw_ui.h con pulsador, LED y USB.
# La usan siempre los programas de banco; el firmware principal usa esta o el panel (REDUX_UI).
set(REDUX_UI_SOURCES
        src/app/ui_basic.c
)

# Fuentes de la UI del panel (desde ../UI): código propio de la línea UI + el puente del lado
# del firmware (ui_panel_fw.c implementa fw_ui.h y el contrato cruzado app/ui.h). LVGL y el
# export de SquareLine se agregan como librerías aparte más abajo (solo con REDUX_UI=PANEL).
set(REDUX_UI_PANEL_FW_SOURCE src/app/ui_panel_fw.c)
set(REDUX_UI_PANEL_UISOURCES
        ${REDUX_UI_DIR}/src/hal/hal_spi.c
        ${REDUX_UI_DIR}/src/hal/hal_keypad.c
        ${REDUX_UI_DIR}/src/hal/hal_keypad_logic.c
        ${REDUX_UI_DIR}/src/hal/hal_wifi.c
        ${REDUX_UI_DIR}/src/hal/hal_api.c
        ${REDUX_UI_DIR}/src/ui/st7735.c
        ${REDUX_UI_DIR}/src/ui/lv_port_disp.c
        ${REDUX_UI_DIR}/src/ui/lv_port_indev.c
        ${REDUX_UI_DIR}/src/ui/ui_panel.c
        ${REDUX_UI_DIR}/src/ui/ui_core1.c
)

# UI del firmware principal según REDUX_UI.
if(REDUX_UI STREQUAL "PANEL")
        set(REDUX_MAIN_UI_SOURCES ${REDUX_UI_PANEL_FW_SOURCE} ${REDUX_UI_PANEL_UISOURCES})
else()
        set(REDUX_MAIN_UI_SOURCES ${REDUX_UI_SOURCES})
endif()

# Lógica de control pura (control/). C portable: solo config/util, nunca el SDK.
set(REDUX_CONTROL_SOURCES
        src/control/intent.c
        src/control/limits.c
        src/control/power.c
        src/control/rep_stats.c
)

# Capa de aplicación (app/). Supervisor (lógica pura) e integración del ciclo.
set(REDUX_APP_SOURCES
        src/app/supervisor.c
        src/app/angle_watch.c
        src/app/session.c
        src/app/app.c
)

# Todas nuestras fuentes (nunca las del SDK): a estas se les aplican las flags
# estrictas y los includes de las capas. Cada fase agrega sus .c aquí.
set(REDUX_SOURCES
        src/main.c
        Pruebas/banco/fase1_hal/fase1_check.c
        Pruebas/banco/fase2_motor/fase2_check.c
        Pruebas/banco/fase3_encoder/fase3_check.c
        Pruebas/banco/emg/emg_check.c
        ${REDUX_HAL_SOURCES}
        ${REDUX_ACTUATOR_SOURCES}
        ${REDUX_SENSOR_SOURCES}
        src/sensors/enc_sim.c
        ${REDUX_UI_SOURCES}
        src/app/ui_panel_fw.c
        ${REDUX_CONTROL_SOURCES}
        ${REDUX_APP_SOURCES}
)

# Directorios de cabeceras de las capas del proyecto.
set(REDUX_INCLUDE_DIRS
        ${CMAKE_CURRENT_LIST_DIR}/src
        ${CMAKE_CURRENT_LIST_DIR}/src/common
        ${CMAKE_CURRENT_LIST_DIR}/src/hal
        ${CMAKE_CURRENT_LIST_DIR}/src/sensors
        ${CMAKE_CURRENT_LIST_DIR}/src/actuators
        ${CMAKE_CURRENT_LIST_DIR}/src/control
        ${CMAKE_CURRENT_LIST_DIR}/src/app
)

# Librerías del SDK que usa el HAL de la Fase 1.
set(REDUX_SDK_LIBS
        pico_stdlib
        hardware_adc
        hardware_pwm
        hardware_i2c
        hardware_gpio
        hardware_timer
        hardware_watchdog
        pico_rand
)

# Configura un ejecutable del proyecto con includes, librerías del SDK y salida USB.
function(redux_configure_target target)
        target_include_directories(${target} PRIVATE ${REDUX_INCLUDE_DIRS})
        target_link_libraries(${target} ${REDUX_SDK_LIBS})
        pico_enable_stdio_uart(${target} 0)
        pico_enable_stdio_usb(${target} 1)
        pico_add_extra_outputs(${target})
endfunction()

# ---------------------------------------------------------------------------
# UI del panel (REDUX_UI=PANEL): LVGL 8.3.11 + export de SquareLine, desde ../UI.
# Mismo esquema que el CMakeLists del laboratorio: LVGL con includes SYSTEM (sus avisos no
# cuentan) y la librería "ui" del export con su propio CMakeLists.
# ---------------------------------------------------------------------------
if(REDUX_UI STREQUAL "PANEL")
        file(GLOB_RECURSE LVGL_SOURCES CONFIGURE_DEPENDS ${REDUX_UI_DIR}/lib/lvgl/src/*.c)
        add_library(lvgl STATIC ${LVGL_SOURCES})
        target_compile_definitions(lvgl PUBLIC LV_CONF_INCLUDE_SIMPLE)
        target_include_directories(lvgl SYSTEM PUBLIC
                ${REDUX_UI_DIR}/lib       # "lvgl/lvgl.h"
                ${REDUX_UI_DIR}/src/ui    # "lv_conf.h"
        )
        target_link_libraries(lvgl PUBLIC pico_stdlib)

        add_subdirectory(${REDUX_UI_DIR}/src/ui/squareline ${CMAKE_BINARY_DIR}/squareline)
        target_link_libraries(ui PUBLIC lvgl)
endif()

# Firmware principal: integra todas las capas (app -> control/sensors/actuators -> hal).
# El encoder (REDUX_ENC) y la UI (REDUX_UI) entran por las listas condicionales de arriba.
add_executable(redux-firmware
        src/main.c
        ${REDUX_APP_SOURCES}
        ${REDUX_CONTROL_SOURCES}
        ${REDUX_SENSOR_SOURCES_COMMON}
        ${REDUX_ENC_SOURCE}
        ${REDUX_ACTUATOR_SOURCES}
        ${REDUX_MAIN_UI_SOURCES}
        ${REDUX_HAL_SOURCES}
)
pico_set_program_name(redux-firmware "redux-firmware")
pico_set_program_version(redux-firmware "0.6")
redux_configure_target(redux-firmware)
# Timeout bajo de la salida USB: una terminal abierta que no lee no bloquea el lazo
# ni dispara el watchdog (el reporte se arma y envía por chunks, ver main.c).
target_compile_definitions(redux-firmware PRIVATE PICO_STDIO_USB_STDOUT_TIMEOUT_US=1000)
# Fuente de EMG por canal: traduce las opciones REDUX_EMG_BIC/TRI a los macros que lee
# config.h. config.h usa #ifndef, así que sin estas opciones queda el default (SERIAL).
target_compile_definitions(redux-firmware PRIVATE
        EMG_SOURCE_BIC=EMG_SOURCE_${REDUX_EMG_BIC}
        EMG_SOURCE_TRI=EMG_SOURCE_${REDUX_EMG_TRI})

# Encoder simulado: activa las ramas #if REDUX_ENC_SIM (sensors/enc_sim, integración del duty).
if(REDUX_ENC STREQUAL "SIM")
        target_compile_definitions(redux-firmware PRIVATE REDUX_ENC_SIM=1)
endif()

# UI del panel: activa las ramas #if REDUX_UI_PANEL (app.c, ui_panel_fw) y suma la línea UI.
if(REDUX_UI STREQUAL "PANEL")
        target_compile_definitions(redux-firmware PRIVATE REDUX_UI_PANEL=1)
        # Raíz de includes de la línea UI ("ui/ui_config.h", "hal/hal_wifi.h", "lwipopts.h",
        # "ui/wifi_redes.h"). board.h y app/ui.h salen de src/ del firmware (va primero).
        target_include_directories(redux-firmware PRIVATE ${REDUX_UI_DIR}/src)
        target_link_libraries(redux-firmware
                hardware_spi
                hardware_dma
                pico_multicore
                pico_cyw43_arch_lwip_threadsafe_background
                lvgl
                ui)
        # Código propio de la línea UI: mismas flags estrictas que en su laboratorio.
        set_source_files_properties(${REDUX_UI_PANEL_UISOURCES} PROPERTIES
                COMPILE_OPTIONS "-Wall;-Wextra;-Werror;-Wshadow;-Wconversion;-Wdouble-promotion;-Wstrict-prototypes;-Wmissing-prototypes")
endif()

# Programas de banco (Pruebas/banco/*): ejecutables de prueba sobre la placa, aparte del
# firmware. Se compilan solo si REDUX_BUILD_BANCOS está en ON (ver opción más arriba).
if(REDUX_BUILD_BANCOS)
        # Programa de prueba de la Fase 1 (bring-up del HAL), ejecutable aparte.
        add_executable(fase1_check Pruebas/banco/fase1_hal/fase1_check.c ${REDUX_HAL_SOURCES})
        pico_set_program_name(fase1_check "fase1_check")
        pico_set_program_version(fase1_check "0.1")
        redux_configure_target(fase1_check)

        # Programa de banco de la Fase 2 (driver BTS7960 con motor sin carga), ejecutable aparte.
        add_executable(fase2_check Pruebas/banco/fase2_motor/fase2_check.c ${REDUX_ACTUATOR_SOURCES} ${REDUX_HAL_SOURCES})
        pico_set_program_name(fase2_check "fase2_check")
        pico_set_program_version(fase2_check "0.1")
        redux_configure_target(fase2_check)

        # Programa de banco de la Fase 3 (AS5600 y calibración del rango), ejecutable aparte.
        # El driver del motor queda deshabilitado; solo se usa para dejarlo en estado seguro.
        add_executable(fase3_check
                Pruebas/banco/fase3_encoder/fase3_check.c
                ${REDUX_SENSOR_SOURCES}
                ${REDUX_UI_SOURCES}
                ${REDUX_ACTUATOR_SOURCES}
                ${REDUX_HAL_SOURCES}
        )
        pico_set_program_name(fase3_check "fase3_check")
        pico_set_program_version(fase3_check "0.1")
        redux_configure_target(fase3_check)

        # Programa de banco del canal EMG y su calibración, ejecutable aparte.
        # El driver del motor queda deshabilitado; solo lee los dos canales de EMG por ADC.
        add_executable(emg_check
                Pruebas/banco/emg/emg_check.c
                ${REDUX_SENSOR_SOURCES}
                ${REDUX_ACTUATOR_SOURCES}
                ${REDUX_HAL_SOURCES}
        )
        pico_set_program_name(emg_check "emg_check")
        pico_set_program_version(emg_check "0.1")
        redux_configure_target(emg_check)
endif()

# Los avisos se tratan como errores desde el día uno, solo sobre nuestras fuentes
# (las librerías INTERFACE del SDK agregan sus .c a estos targets, así que las flags
# se scopean por archivo). Ver docs/PLAN.md, sección "Estilo y herramientas".
set_source_files_properties(${REDUX_SOURCES} PROPERTIES
        COMPILE_OPTIONS "-Wall;-Wextra;-Werror;-Wshadow;-Wconversion"
)

