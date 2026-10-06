# tests/CMakeLists.txt · Build de las pruebas unitarias para la PC (gcc nativo + Unity).
# Compila la lógica pura de control/ y las conversiones de los drivers con mocks de hal_*.
# Proyecto independiente del firmware: NO usa el Pico SDK. Se completa a medida que cada
# fase agrega su test_<modulo>.c.
#
# Uso (desde la raíz del repo):
#   cmake -B build-tests -S tests -G Ninja
#   cmake --build build-tests
#   ctest --test-dir build-tests --output-on-failure

cmake_minimum_required(VERSION 3.13)
project(redux-tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

enable_testing()

set(REDUX_ROOT ${CMAKE_CURRENT_LIST_DIR}/..)

# Framework Unity (vendorizado en tests/unity). Sus propios avisos no son nuestros,
# así que se compila sin las flags estrictas.
add_library(unity STATIC unity/unity.c)
target_include_directories(unity PUBLIC unity)

# Directorios de cabeceras portables del proyecto + los mocks de hal_*.
set(TEST_INCLUDE_DIRS
        ${REDUX_ROOT}/src/common
        ${REDUX_ROOT}/src/hal
        ${REDUX_ROOT}/src/sensors
        ${REDUX_ROOT}/src/actuators
        ${REDUX_ROOT}/src/control
        ${REDUX_ROOT}/src/app
        ${CMAKE_CURRENT_LIST_DIR}/mocks
)

# Flags estrictas solo sobre nuestras fuentes (igual que el firmware).
set(REDUX_TEST_FLAGS -Wall -Wextra -Werror -Wshadow -Wconversion)

# Declara un ejecutable de prueba y lo registra en ctest.
# Uso: redux_add_test(<nombre> <fuentes...>)
function(redux_add_test name)
        add_executable(${name} ${ARGN})
        target_include_directories(${name} PRIVATE ${TEST_INCLUDE_DIRS})
        target_compile_options(${name} PRIVATE ${REDUX_TEST_FLAGS})
        target_link_libraries(${name} PRIVATE unity)
        add_test(NAME ${name} COMMAND ${name})
endfunction()

# --- Fase 2 · driver BTS7960 -------------------------------------------------
redux_add_test(test_bts7960
        test_bts7960.c
        ${REDUX_ROOT}/src/actuators/bts7960.c
        mocks/mock_hal_pwm.c
        mocks/mock_hal_gpio.c
)

# --- Fase 3 · driver AS5600 y calibración del rango --------------------------
redux_add_test(test_as5600
        test_as5600.c
        ${REDUX_ROOT}/src/sensors/as5600.c
        mocks/mock_hal_i2c.c
)

redux_add_test(test_rom_calib
        test_rom_calib.c
        ${REDUX_ROOT}/src/sensors/rom_calib.c
        ${REDUX_ROOT}/src/sensors/as5600.c
        mocks/mock_hal_i2c.c
)

# --- Fase 4 · canal EMG y calibración ----------------------------------------
# emg_calib usa sqrt (math.h); en gcc nativo hay que enlazar libm.
redux_add_test(test_emg
        test_emg.c
        ${REDUX_ROOT}/src/sensors/emg.c
        ${REDUX_ROOT}/src/sensors/emg_calib.c
)
target_link_libraries(test_emg PRIVATE m)

# --- Fase 5 · lógica de control pura (intent, limits, power) ------------------
redux_add_test(test_intent
        test_intent.c
        ${REDUX_ROOT}/src/control/intent.c
)

redux_add_test(test_limits
        test_limits.c
        ${REDUX_ROOT}/src/control/limits.c
)

redux_add_test(test_power
        test_power.c
        ${REDUX_ROOT}/src/control/power.c
)

# Simulación encadenada: usa los tres módulos de control juntos.
redux_add_test(test_control_chain
        test_control_chain.c
        ${REDUX_ROOT}/src/control/intent.c
        ${REDUX_ROOT}/src/control/limits.c
        ${REDUX_ROOT}/src/control/power.c
)

# --- Fase 6 · supervisor, detector de salto de ángulo e integración ----------
# El supervisor es lógica pura; el mini-test de integración encadena supervisor con
# intent/limits/power (la misma cadena que app_tick corre en ST_RUN, sin hardware).
redux_add_test(test_supervisor
        test_supervisor.c
        ${REDUX_ROOT}/src/app/supervisor.c
        ${REDUX_ROOT}/src/app/angle_watch.c
        ${REDUX_ROOT}/src/control/intent.c
        ${REDUX_ROOT}/src/control/limits.c
        ${REDUX_ROOT}/src/control/power.c
)
