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

### Si el `.uf2` no se genera (picotool bloqueado por Device Guard)

En esta máquina, Device Guard / Smart App Control a veces bloquea `picotool.exe`, el
paso de empaquetado que CMake invoca al final. Cuando pasa, el enlace del `.elf` igual
se completa (`build/redux-firmware.elf` queda bien), pero el build corta sin producir el
`.uf2`. Para generarlo sin tocar la configuración de Windows, se usa un `picotool`
nativo de Linux compilado en WSL 2:

```powershell
tools\make_uf2_wsl.ps1
```

La primera vez compila `picotool` dentro de WSL (en `~/.local/bin`, no en el repo),
usando como `PICO_SDK_PATH` el SDK ya instalado en Windows; después solo convierte. Al
terminar imprime `picotool info` del `.uf2` (debe decir `target chip: RP2350`,
`image type: ARM Secure`, con el nombre y la versión del programa). Para otro ejecutable:
`tools\make_uf2_wsl.ps1 -Elf build\fase2_check.elf -Uf2 build\fase2_check.uf2`.

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
