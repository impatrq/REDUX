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
)

# Drivers de actuadores (actuators/). C portable: usan solo el HAL, nunca el SDK.
set(REDUX_ACTUATOR_SOURCES
        src/actuators/bts7960.c
)

# Drivers de sensores (sensors/). C portable: usan solo el HAL, nunca el SDK.
set(REDUX_SENSOR_SOURCES
        src/sensors/as5600.c
        src/sensors/rom_calib.c
        src/sensors/emg.c
        src/sensors/emg_calib.c
)

# Interfaz de usuario (app/). ui_basic implementa ui.h con pulsador, LED y USB.
set(REDUX_UI_SOURCES
        src/app/ui_basic.c
)

# Todas nuestras fuentes (nunca las del SDK): a estas se les aplican las flags
# estrictas y los includes de las capas. Cada fase agrega sus .c aquí.
set(REDUX_SOURCES
        src/main.c
        src/diag/fase1_check.c
        src/diag/fase2_check.c
        src/diag/fase3_check.c
        src/diag/fase4_check.c
        ${REDUX_HAL_SOURCES}
        ${REDUX_ACTUATOR_SOURCES}
        ${REDUX_SENSOR_SOURCES}
        ${REDUX_UI_SOURCES}
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
)

# Configura un ejecutable del proyecto con includes, librerías del SDK y salida USB.
function(redux_configure_target target)
        target_include_directories(${target} PRIVATE ${REDUX_INCLUDE_DIRS})
        target_link_libraries(${target} ${REDUX_SDK_LIBS})
        pico_enable_stdio_uart(${target} 0)
        pico_enable_stdio_usb(${target} 1)
        pico_add_extra_outputs(${target})
endfunction()

# Firmware principal.
add_executable(redux-firmware src/main.c ${REDUX_HAL_SOURCES})
pico_set_program_name(redux-firmware "redux-firmware")
pico_set_program_version(redux-firmware "0.1")
redux_configure_target(redux-firmware)

# Programa de prueba de la Fase 1 (bring-up del HAL), ejecutable aparte.
add_executable(fase1_check src/diag/fase1_check.c ${REDUX_HAL_SOURCES})
pico_set_program_name(fase1_check "fase1_check")
pico_set_program_version(fase1_check "0.1")
redux_configure_target(fase1_check)

# Programa de banco de la Fase 2 (driver BTS7960 con motor sin carga), ejecutable aparte.
add_executable(fase2_check src/diag/fase2_check.c ${REDUX_ACTUATOR_SOURCES} ${REDUX_HAL_SOURCES})
pico_set_program_name(fase2_check "fase2_check")
pico_set_program_version(fase2_check "0.1")
redux_configure_target(fase2_check)

# Programa de banco de la Fase 3 (AS5600 y calibración del rango), ejecutable aparte.
# El driver del motor queda deshabilitado; solo se usa para dejarlo en estado seguro.
add_executable(fase3_check
        src/diag/fase3_check.c
        ${REDUX_SENSOR_SOURCES}
        ${REDUX_UI_SOURCES}
        ${REDUX_ACTUATOR_SOURCES}
        ${REDUX_HAL_SOURCES}
)
pico_set_program_name(fase3_check "fase3_check")
pico_set_program_version(fase3_check "0.1")
redux_configure_target(fase3_check)

# Programa de banco de la Fase 4 (canal EMG y calibración), ejecutable aparte.
# El driver del motor queda deshabilitado; solo lee los dos canales de EMG por ADC.
add_executable(fase4_check
        src/diag/fase4_check.c
        ${REDUX_SENSOR_SOURCES}
        ${REDUX_ACTUATOR_SOURCES}
        ${REDUX_HAL_SOURCES}
)
pico_set_program_name(fase4_check "fase4_check")
pico_set_program_version(fase4_check "0.1")
redux_configure_target(fase4_check)

# Los avisos se tratan como errores desde el día uno, solo sobre nuestras fuentes
# (las librerías INTERFACE del SDK agregan sus .c a estos targets, así que las flags
# se scopean por archivo). Ver docs/PLAN.md, sección "Estilo y herramientas".
set_source_files_properties(${REDUX_SOURCES} PROPERTIES
        COMPILE_OPTIONS "-Wall;-Wextra;-Werror;-Wshadow;-Wconversion"
)

