# Pruebas — firmware REDUX

Todas las pruebas del exoesqueleto, separadas del firmware real (que vive en `src/`).
Hay dos tipos:

- **`unitarias/`** — pruebas de la lógica portable, corren en la **PC** con Unity y mocks
  del HAL (sin hardware ni Pico SDK). Son las que se corren antes de cada carga a la placa.
- **`banco/`** — programas de prueba que corren **sobre la placa** (Pico 2 W), uno por
  fase, cada uno su propio ejecutable. Se usan para verificar el hardware real en
  protoboard: HAL, motor, encoder y EMG.

## Unitarias (PC)

Forma oficial (WSL):

```powershell
powershell -ExecutionPolicy Bypass -File Pruebas\unitarias\run_tests_wsl.ps1
```

Detalle y lista de tests en [`unitarias/README.md`](unitarias/README.md).

## Banco (sobre la placa)

| Carpeta | Target | Qué prueba | Comandos serie |
| --- | --- | --- | --- |
| [`banco/fase1_hal/`](banco/fase1_hal/README.md) | `fase1_check` | HAL: tick, pulsador, ADC, PWM del LED. Motor off | (solo imprime) |
| [`banco/fase2_motor/`](banco/fase2_motor/README.md) | `fase2_check` | Driver BTS7960 con motor sin carga | `f N` / `e N` / `s` / `en 1\|0` |
| [`banco/fase3_encoder/`](banco/fase3_encoder/README.md) | `fase3_check` | Encoder AS5600 y calibración del rango. Motor off | `cal` / `raw` / `ok` / `cancel` / `recal` |
| [`banco/emg/`](banco/emg/README.md) | `emg_check` | Canal EMG y calibración. Motor off | `rest` / `mvc b` / `mvc t` |

Cada carpeta tiene su `README.md` con las conexiones y cómo cargarlo. Los targets de banco
se compilan junto con el firmware (opción `REDUX_BUILD_BANCOS`, ON por defecto); el `.uf2`
de cada uno se genera con `tools\make_uf2_wsl.ps1 -Elf build\<target>.elf -Uf2 build\<target>.uf2`.

## Otros
- [`telemetry_plot.py`](telemetry_plot.py) — lee el CSV de telemetría por USB y lo grafica
  en vivo (útil con el banco `emg`).
