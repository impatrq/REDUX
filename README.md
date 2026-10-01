<!-- README.md · Cómo compilar, cargar y usar el firmware. -->

# REDUX firmware

Firmware en C11 del exoesqueleto de codo REDUX, sobre Raspberry Pi Pico 2 W (RP2350)
y Pico SDK 2.x. EMG bíceps/tríceps → lógica XOR → BTS7960, con encoder AS5600 por I2C
para los límites articulares.

El plan completo de implementación está en [docs/PLAN.md](docs/PLAN.md), y las reglas de
trabajo con Claude Code en [CLAUDE.md](CLAUDE.md).

## Compilar el firmware

```sh
cmake -B build -G Ninja -DPICO_BOARD=pico2_w
cmake --build build
```

## Pruebas en la PC

```sh
cmake -B build-tests -S tests
cmake --build build-tests
ctest --test-dir build-tests
```

## Cargar en la Pico

```sh
tools/flash.sh
```
