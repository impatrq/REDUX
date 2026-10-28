<!-- README.md · Presentación del repo: qué es, hardware, compilar, uf2, cargar y uso. -->

# REDUX firmware

Firmware en C11 del **exoesqueleto de codo REDUX**, sobre Raspberry Pi Pico 2 W (RP2350)
y Pico SDK 2.x. Lee la actividad muscular de **bíceps y tríceps** por EMG, decide la
intención con una **lógica XOR**, y mueve el codo con un motor a través de un puente H
**BTS7960**. Un **encoder magnético AS5600** (I2C) mide el ángulo para respetar los
límites articulares. La seguridad es lo primero: co-contracción, límite alcanzado o falla
del encoder cortan el motor de inmediato.

El plan de desarrollo por fases está en [docs/PLAN.md](docs/PLAN.md); el detalle de
hardware en [docs/hardware.md](docs/hardware.md) y la máquina de estados de seguridad en
[docs/state_machine.md](docs/state_machine.md).

## Estructura del repo

```
src/        Firmware real (app → control / sensors / actuators → hal → SDK)
Pruebas/    Pruebas unitarias (PC) y programas de banco (placa) — ver Pruebas/README.md
tools/      Scripts: make_uf2_wsl.ps1, flash.sh, make_entregas.ps1
docs/       Plan, hardware y máquina de estados
```

## Hardware y pinout

Placa: Raspberry Pi Pico 2 W (RP2350). `src/hal/board.h` y [docs/hardware.md](docs/hardware.md)
son la única fuente de verdad del pinout.

| Señal | GPIO | Notas |
| --- | --- | --- |
| EMG bíceps | GP26 / ADC0 | Señal rectificada (onda completa), sin suavizar, 0–3 V |
| EMG tríceps | GP27 / ADC1 | Ídem |
| BTS7960 RPWM (flexión) | GP8 | PWM 20 kHz, vía 6N137 (no invierte) |
| BTS7960 LPWM (extensión) | GP9 | PWM 20 kHz, vía 6N137 (no invierte) |
| AS5600 SDA / SCL | GP4 / GP5 | I2C0 a 400 kHz, dirección 0x36 |
| LED de estado | GP2 | Solo desarrollo (lo reemplaza la pantalla) |
| Pulsador | GP3 | Solo desarrollo, pull-up interno, activo en bajo |

- **R_EN y L_EN del BTS7960 van fijos a +5 V** del lado del motor: el firmware no los
  maneja. El corte del motor es siempre **RPWM = LPWM = 0**.
- Dos masas aisladas (GND1 potencia / GND2 lógica); el único cruce son RPWM/LPWM por
  optoacopladores 6N137. Con la Pico apagada, los 6N137 dejan ambas señales en alto =
  **freno** (arranque seguro). Detalles en [docs/hardware.md](docs/hardware.md).

## Compilar el firmware

```sh
cmake -B build -G Ninja -DPICO_BOARD=pico2_w
cmake --build build
```

Esto compila el firmware (`redux-firmware`) y los programas de banco (`fase1_check`,
`fase2_check`, `fase3_check`, `emg_check`). Para compilar **solo el firmware**, sin los
bancos:

```sh
cmake -B build -G Ninja -DPICO_BOARD=pico2_w -DREDUX_BUILD_BANCOS=OFF
cmake --build build
```

### Generar el `.uf2` (picotool bloqueado por Device Guard)

En esta máquina, Device Guard / Smart App Control a veces bloquea `picotool.exe`, el paso
de empaquetado que CMake invoca al final: el `.elf` se enlaza bien (`build/redux-firmware.elf`),
pero el build corta sin producir el `.uf2`. Para generarlo se usa un `picotool` nativo de
Linux compilado en WSL 2:

```powershell
tools\make_uf2_wsl.ps1
```

La primera vez compila `picotool` dentro de WSL (en `~/.local/bin`, no en el repo), usando
como `PICO_SDK_PATH` el SDK ya instalado en Windows; después solo convierte. Para otro
ejecutable (por ejemplo un banco):

```powershell
tools\make_uf2_wsl.ps1 -Elf build\emg_check.elf -Uf2 build\emg_check.uf2
```

## Cargar en la Pico

Poner la Pico en modo BOOTSEL (mantener el botón al conectar) y copiar el `.uf2` a la
unidad `RPI-RP2`, o bien:

```sh
tools/flash.sh
```

## Pruebas

- **Unitarias (PC, Unity):** la forma oficial es por WSL —
  `powershell -ExecutionPolicy Bypass -File Pruebas\unitarias\run_tests_wsl.ps1`.
- **De banco (sobre la placa):** un programa por fase en `Pruebas/banco/`.

Todo el detalle en [Pruebas/README.md](Pruebas/README.md).

## Comandos serie del firmware

Por el monitor serie USB (una línea por comando):

| Comando | Efecto |
| --- | --- |
| `b N` | Intensidad simulada de bíceps (flexión) al N % (0–100) |
| `t N` | Intensidad simulada de tríceps (extensión) al N % |
| `r` | Reposo: ambos canales a 0 |
| `ok` | Confirmar (arranque / calibración) |
| `cancel` | Cancelar el paso actual |
| `recal` | Volver a calibrar desde el rango (ROM) |
| `hang` | Solo en modo banco: cuelga el lazo para probar el watchdog |

En el banco (sin electrodos) la fuente de EMG es **simulada por serie** (`b`/`t`/`r`); con
el paciente conectado se pasa a ADC (ver `EMG_SOURCE` en `src/common/config.h`). El
firmware emite un reporte de estado por USB periódicamente.

## Estados (supervisor de seguridad)

```
BOOT → CHECK_ENC → CALIB_ROM → CALIB_REST → CALIB_MVC → ARMING → RUN
                                                                    ↕
                                                                  FAULT
```

El motor **solo recibe PWM en `RUN`**. Cualquier falla (encoder, saturación de EMG,
sobrepaso del ciclo) lleva a `FAULT` con corte inmediato; el rearme exige lecturas
estables y ambos músculos en reposo. Transiciones completas en
[docs/state_machine.md](docs/state_machine.md).

## Abrir el proyecto en VS Code

La carpeta `.vscode/` **no se versiona** (es configuración local de cada máquina), así que
al clonar el repo no viene incluida. Dos formas de trabajar:

- **Extensión Raspberry Pi Pico:** instalar la extensión *Raspberry Pi Pico* en VS Code y
  usar *Pico: Import Project* sobre la carpeta del repo (board `pico2_w`, Pico SDK 2.x). La
  extensión regenera su propia configuración de `.vscode/` y los comandos de build/flash.
- **Por línea de comandos:** no hace falta VS Code. Alcanza con tener instalados el Pico
  SDK 2.x, CMake y Ninja, y usar los comandos de *Compilar el firmware* de arriba.
