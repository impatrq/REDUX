# REDUX firmware

Exoesqueleto de codo. Raspberry Pi Pico 2 W (RP2350), C11, Pico SDK 2.x.
EMG bíceps/tríceps → lógica XOR → BTS7960. Encoder AS5600 por I2C para límites.

## Comandos
- Firmware: cmake -B build -G Ninja -DPICO_BOARD=pico2_w && cmake --build build
- Pruebas PC (oficial): tools/run_tests_wsl.ps1 — compila y corre las pruebas en WSL 2
  (Ubuntu: gcc/cmake/ninja), build separado en build-tests-wsl/. Se usa WSL porque Smart
  App Control bloquea los .exe de prueba de MinGW en Windows.
- Pruebas PC (sin WSL, puede quedar bloqueado por SAC): cmake -B build-tests -S tests && cmake --build build-tests && ctest --test-dir build-tests
- Carga: tools/flash.sh

## Arquitectura (ver docs/PLAN.md)
- Capas: app → control / sensors / actuators → hal → SDK. Las dependencias solo bajan.
- Solo hal/ y main.c incluyen headers del SDK.
- control/ es lógica pura y se prueba en PC.

## Reglas de seguridad (no negociables)
- RPWM y LPWM nunca con duty > 0 a la vez.
- Frenar a 0 antes de invertir el sentido.
- Co-contracción, límite alcanzado o falla del encoder → corte inmediato.
- Rearme solo con ambos EMG en reposo.
- Toda espera de hardware tiene timeout.

## Convenciones
- snake_case con prefijo de módulo; tipos con _t; constantes con unidad (LIM_MAX_DEG).
- Sin malloc, sin números mágicos (todo en config.h), internos static.
- Cada módulo nuevo llega con tests/test_<modulo>.c.
- Formatear con clang-format; compilar sin avisos.

## Cómo trabajar
- Responder siempre en español.
- Una fase por sesión; no tocar archivos de otras fases sin avisar.
- Antes de terminar: compilar firmware, correr pruebas, resumir cambios.
- Commits: feat(modulo): ..., fix(modulo): ..., test(modulo): ...
- Nunca hacer git push ni configurar remotos; los commits quedan solo en local.
