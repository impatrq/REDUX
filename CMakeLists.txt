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

# Add executable. Default name is the project name, version 0.1
# Fuentes propias del firmware. Cada fase agrega sus .c a esta lista
# (ver docs/PLAN.md). Las flags estrictas de abajo se aplican solo a estos
# archivos, nunca a las fuentes del SDK.
set(REDUX_SOURCES
        src/main.c
)

add_executable(redux-firmware ${REDUX_SOURCES})

pico_set_program_name(redux-firmware "redux-firmware")
pico_set_program_version(redux-firmware "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(redux-firmware 0)
pico_enable_stdio_usb(redux-firmware 1)

# Add the standard library to the build
target_link_libraries(redux-firmware
        pico_stdlib)

# Add the standard include files to the build
target_include_directories(redux-firmware PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/src
        ${CMAKE_CURRENT_LIST_DIR}/src/common
        ${CMAKE_CURRENT_LIST_DIR}/src/hal
        ${CMAKE_CURRENT_LIST_DIR}/src/sensors
        ${CMAKE_CURRENT_LIST_DIR}/src/actuators
        ${CMAKE_CURRENT_LIST_DIR}/src/control
        ${CMAKE_CURRENT_LIST_DIR}/src/app
)

# Los avisos se tratan como errores desde el día uno, solo sobre nuestras fuentes
# (las librerías INTERFACE del SDK agregan sus .c a este target, así que las flags
# se scopean por archivo). Ver docs/PLAN.md, sección "Estilo y herramientas".
set_source_files_properties(${REDUX_SOURCES} PROPERTIES
        COMPILE_OPTIONS "-Wall;-Wextra;-Werror;-Wshadow;-Wconversion"
)

# Add any user requested libraries
target_link_libraries(redux-firmware

        )

pico_add_extra_outputs(redux-firmware)

