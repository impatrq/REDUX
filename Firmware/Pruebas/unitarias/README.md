# Pruebas unitarias (PC)

Pruebas de la lógica portable del firmware, compiladas y corridas en la **PC** con gcc
nativo y el framework **Unity** (vendorizado en `unity/`). No usan el Pico SDK: ejercitan
la lógica pura de `control/` y las conversiones de los drivers, con **mocks** del HAL
(`mocks/`) en lugar de hardware. Es un proyecto CMake independiente del firmware.

## Forma oficial: WSL

En esta máquina, Smart App Control (Enforced) bloquea los `.exe` de prueba recién
enlazados por MinGW, así que las pruebas se corren en **WSL 2** (Ubuntu: gcc, cmake,
ninja). El build va a `build-tests-wsl/` en la raíz del repo.

```powershell
powershell -ExecutionPolicy Bypass -File Pruebas\unitarias\run_tests_wsl.ps1
# Pruebas\unitarias\run_tests_wsl.ps1 -Clean   # borra build-tests-wsl antes de configurar
```

Devuelve el código de salida de `ctest` (0 = todas pasaron).

## Sin WSL (puede quedar bloqueado por SAC)

```sh
cmake -B build-tests -S Pruebas/unitarias -G Ninja
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```

## Qué cubre cada prueba
| Test | Módulo |
| --- | --- |
| `test_bts7960` | Driver BTS7960: exclusión RPWM/LPWM, freno antes de invertir |
| `test_as5600` | Driver del encoder AS5600 (I2C mockeado) |
| `test_rom_calib` | Calibración del rango de movimiento (ROM) |
| `test_intent` | Lógica de intención (XOR de los dos EMG) |
| `test_limits` | Límites por ángulo (zona de límite y corte) |
| `test_power` | Potencia: intensidad EMG → duty con rampas y topes |
| `test_control_chain` | Integración de la cadena intent → limits → power |
| `test_emg` | Canal EMG: envolvente, activo, picos y saturación |
| `test_supervisor` | Supervisor: transiciones, rearme y fallas |
| `test_motor_output` | Salida al motor: control + límites → BTS7960 (mocks) |

Para correr una sola: `ctest --test-dir build-tests-wsl -R test_power`.

## Estructura
- `test_*.c` — las pruebas, una por módulo.
- `mocks/` — mocks del HAL (`hal_gpio`, `hal_i2c`, `hal_pwm`) para aislar los drivers.
- `unity/` — framework Unity (no se toca).
- `CMakeLists.txt` — define cada target `test_<modulo>` y lo registra en ctest.

Convención del proyecto: cada módulo nuevo llega con su `Pruebas/unitarias/test_<modulo>.c`.
