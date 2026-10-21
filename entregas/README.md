# Entregas por fase - firmware REDUX

Exoesqueleto de codo. Raspberry Pi Pico 2 W (RP2350), C11, Pico SDK 2.x.

Cada zip contiene el codigo, las pruebas y el diagnostico de una fase, mas un `LEEME.md` que explica que hace la fase, lista sus archivos, como correr sus pruebas y los commits que la componen. Los zips se regeneran con `tools/make_entregas.ps1` y no se versionan (estan en .gitignore); este indice y el script si.

Nota: la numeracion interna del repo cruza control y EMG (el commit de control dice 'Fase 5', el de EMG 'Fase 4', y el banco se llama fase4_check). Estas entregas se numeran por contenido: fase4 = control, fase5 = EMG.

## Indice

| Fase | Zip | Tag | Contenido |
|------|-----|-----|-----------|
| Fase 0 - Estructura del proyecto | `fase0_estructura.zip` | `fase-0` | Estructura del proyecto |
| Fase 1 - HAL (capa de abstraccion de hardware) | `fase1_hal.zip` | `fase-1` | HAL (capa de abstraccion de hardware) |
| Fase 2 - Motor (driver BTS7960) | `fase2_motor.zip` | `fase-2` | Motor (driver BTS7960) |
| Fase 3 - Encoder AS5600 y calibracion del rango | `fase3_encoder.zip` | `fase-3` | Encoder AS5600 y calibracion del rango |
| Fase 4 - Control (logica pura) | `fase4_control.zip` | `fase-4` | Control (logica pura) |
| Fase 5 - EMG (canal y calibracion) | `fase5_emg.zip` | `fase-5` | EMG (canal y calibracion) |
| Fase 6 - Supervisor, app y firmware integrado | `fase6_supervisor.zip` | `fase-6` | Supervisor, app y firmware integrado |
| Herramientas y framework de pruebas | `herramientas.zip` | - | Herramientas y framework de pruebas |

### Fase 0 - Estructura del proyecto

- Zip: `fase0_estructura.zip`
- Tag: `fase-0`
- Esqueleto del repositorio: arbol de carpetas por capas, build del firmware, convenciones de codigo, tipos y constantes comunes, y la documentacion base (plan y hardware).
- Commits:
  - c38da4c - chore(fase-0): esqueleto de carpetas, build y convenciones

### Fase 1 - HAL (capa de abstraccion de hardware)

- Zip: `fase1_hal.zip`
- Tag: `fase-1`
- Capa HAL completa sobre el Pico SDK: GPIO, ADC (EMG), I2C (encoder), PWM, tiempo con timeout, watchdog y pulsador. Incluye el banco de bring-up fase1_check que ejercita el HAL sin activar el motor.
- Commits:
  - 0cd6f40 - feat(hal): implementar config, tipos y HAL de la Fase 1
  - fcb2f3c - feat(hal): agregar hal_gpio_motor_enable para el BTS7960 (retoque)
  - e8cb6fd - fix(hal): pinout definitivo de la PCB (retoque de HAL)

### Fase 2 - Motor (driver BTS7960)

- Zip: `fase2_motor.zip`
- Tag: `fase-2`
- Driver del puente H BTS7960 con sus reglas de seguridad (RPWM y LPWM nunca con duty a la vez, freno a 0 antes de invertir), el HAL de PWM que usa, el banco fase2_check para probar el motor sin carga por comandos serie, y las pruebas de PC del driver con sus mocks.
- Commits:
  - 1de50de - feat(bts7960): driver del puente H con pruebas en PC
  - 578fe3c - feat(diag): programa de banco fase2_check del BTS7960
  - fcb2f3c - feat(hal): hal_gpio_motor_enable para el BTS7960
  - e8cb6fd - fix(hal): pinout definitivo de la PCB (enables fijos)

### Fase 3 - Encoder AS5600 y calibracion del rango

- Zip: `fase3_encoder.zip`
- Tag: `fase-3`
- Driver del encoder magnetico AS5600 por I2C, la calibracion del rango de movimiento del paciente (rom_calib), la UI basica que usa el banco, el banco fase3_check y las pruebas de PC del encoder y de la calibracion.
- Commits:
  - 1378570 - feat(sensors): driver AS5600 y calibracion del rango del paciente (Fase 3)
  - d9f967b - feat(diag): mostrar la lectura cruda del AS5600 durante la calibracion

### Fase 4 - Control (logica pura)

- Zip: `fase4_control.zip`
- Tag: `fase-4`
- Logica de control pura y probable en PC: intencion (XOR de los dos EMG), limites por angulo y potencia (intensidad EMG -> duty con rampas y topes). Sin dependencias del hardware; incluye la prueba de integracion de toda la cadena.
- Commits:
  - 661e69f - feat(control): logica pura de intencion, limites y potencia (numerado Fase 5 en el repo)

### Fase 5 - EMG (canal y calibracion)

- Zip: `fase5_emg.zip`
- Tag: `fase-5`
- Procesamiento del canal EMG (envolvente, deteccion de activo e intensidad) y su calibracion por musculo (reposo y contraccion maxima). Incluye el banco fase4_check (asi se llama en el repo, pero ejercita el EMG) y las pruebas de PC del canal.
- Commits:
  - bfffe3c - feat(emg): canal EMG, calibracion y banco fase4_check (numerado Fase 4 en el repo)
  - 8c6d9a6 - fix(emg): corregir test del pico corto y cubrir artefacto por saturacion

### Fase 6 - Supervisor, app y firmware integrado

- Zip: `fase6_supervisor.zip`
- Tag: `fase-6`
- Maquina de estados de seguridad (supervisor) con vigilancia de angulo, el ciclo de aplicacion que integra sensores + control + supervisor + motor + watchdog, el main del firmware, la documentacion de la maquina de estados y las pruebas del supervisor y de la salida al motor.
- Commits:
  - 2a75e87 - feat(supervisor): maquina de estados de seguridad
  - 8cb479c - feat(app): integracion del ciclo y watchdog
  - 0ebb316 - feat(app): firmware con ui y reporte USB
  - 688e566 - test(supervisor): transiciones, rearme y fallas
  - cc57eeb - test(diag): comando hang para probar watchdog
  - 939dac2 - docs(plan): actualizar plan con fase 6
  - a9cddb4 - fix(app): mostrar raw del encoder en el reporte
  - 80da459 - fix(app): inicializar el PWM del motor y reportar el duty aplicado

### Herramientas y framework de pruebas

- Zip: `herramientas.zip`
- No es una fase: reune los scripts de tools/ (pruebas y uf2 por WSL, carga, graficado y este generador de entregas) y el framework de pruebas completo (Unity, todos los mocks del HAL y el CMake de pruebas). Es lo transversal que usan todas las fases.
- Commits:
  - c38da4c - chore(fase-0): tools/ y tests/ iniciales
  - 1de50de - feat(bts7960): Unity y primeros mocks
  - 1378570 - feat(sensors): mock de I2C
  - 35c0ea2 - chore(tests): correr las pruebas de PC en WSL
  - c53d55a - chore(tools): generar uf2 por WSL

## Como regenerar los zips

```powershell
powershell -ExecutionPolicy Bypass -File tools\make_entregas.ps1
```

