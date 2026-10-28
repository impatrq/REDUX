<#
.SYNOPSIS
  Regenera las entregas por fase del firmware REDUX (un zip por fase + indice).

.DESCRIPTION
  Fuente unica de verdad del reparto de archivos por fase. Para cada fase:
    - arma un LEEME.md (que hace, archivos, como correr pruebas, commits),
    - empaqueta el codigo, las pruebas y el diag de esa fase en entregas/<fase>.zip,
  y al final regenera entregas/README.md con el indice de todas las fases.

  Los zips se regeneran siempre desde el codigo actual del repo (HEAD / working tree),
  asi que se pueden actualizar cuando cambie el codigo. Los zips estan en .gitignore;
  entregas/README.md y este script si se versionan.

.EXAMPLE
  pwsh tools/make_entregas.ps1
  powershell -ExecutionPolicy Bypass -File tools\make_entregas.ps1
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

# tools/ cuelga de la raiz del repo
$RepoRoot    = Split-Path -Parent $PSScriptRoot
$EntregasDir = Join-Path $RepoRoot 'entregas'
$StagingRoot = Join-Path $EntregasDir '.staging'

# ---------------------------------------------------------------------------
# Descripcion de una linea por archivo (ruta relativa al repo -> texto)
# ---------------------------------------------------------------------------
$Desc = @{
    'CMakeLists.txt'                = 'Build del firmware (Pico SDK 2.x, define los targets de la app y de los faseN_check).'
    'CLAUDE.md'                     = 'Guia del proyecto: arquitectura por capas, reglas de seguridad y convenciones.'
    'README.md'                     = 'Presentacion del repositorio.'
    'pico_sdk_import.cmake'         = 'Localiza e importa el Pico SDK para el build del firmware.'
    '.clang-format'                 = 'Estilo de formato de codigo (clang-format).'
    'src/common/config.h'           = 'Todas las constantes con unidad (pines, limites, duty, timeouts). Sin numeros magicos.'
    'src/common/redux_types.h'      = 'Tipos compartidos (direccion, estados, estructuras de muestra) con sufijo _t.'
    'src/common/util.h'             = 'Utilidades inline comunes (clamp, mapeos) sin dependencias del SDK.'
    'docs/PLAN.md'                  = 'Plan de desarrollo por fases y decisiones de diseno.'
    'docs/hardware.md'              = 'Pinout, conexiones de la PCB, aislacion 6N137 y masas.'
    'docs/decisions/.gitkeep'       = 'Marcador para versionar la carpeta de decisiones.'
    'docs/state_machine.md'         = 'Maquina de estados de seguridad del supervisor (estados y transiciones).'

    'src/hal/board.h'               = 'Mapa de pines y constantes de placa (RP2350 / Pico 2 W).'
    'src/hal/hal_adc.c'             = 'HAL de ADC: lectura de los dos canales EMG (biceps/triceps).'
    'src/hal/hal_adc.h'             = 'Interfaz del HAL de ADC.'
    'src/hal/hal_button.c'          = 'HAL del pulsador con antirrebote.'
    'src/hal/hal_button.h'          = 'Interfaz del HAL del pulsador.'
    'src/hal/hal_gpio.c'            = 'HAL de GPIO, incluye el enable del driver BTS7960.'
    'src/hal/hal_gpio.h'            = 'Interfaz del HAL de GPIO.'
    'src/hal/hal_i2c.c'             = 'HAL de I2C para el encoder AS5600.'
    'src/hal/hal_i2c.h'             = 'Interfaz del HAL de I2C.'
    'src/hal/hal_pwm.c'             = 'HAL de PWM (LED de estado y canales RPWM/LPWM del motor).'
    'src/hal/hal_pwm.h'             = 'Interfaz del HAL de PWM.'
    'src/hal/hal_time.c'            = 'HAL de tiempo: ticks de 1 ms, esperas con timeout.'
    'src/hal/hal_time.h'            = 'Interfaz del HAL de tiempo.'
    'src/hal/hal_wdt.c'             = 'HAL del watchdog.'
    'src/hal/hal_wdt.h'             = 'Interfaz del HAL del watchdog.'
    'Pruebas/banco/fase1_hal/fase1_check.c'        = 'Banco de bring-up de la Fase 1: ejercita el HAL e imprime por USB. El motor nunca se activa.'

    'src/actuators/bts7960.c'       = 'Driver del puente H BTS7960; garantiza que RPWM y LPWM nunca tengan duty a la vez.'
    'src/actuators/bts7960.h'       = 'Interfaz del driver BTS7960.'
    'Pruebas/banco/fase2_motor/fase2_check.c'        = 'Banco de la Fase 2: maneja el motor sin carga por comandos serie, con hombre muerto y freno al invertir.'

    'src/sensors/as5600.c'          = 'Driver del encoder magnetico AS5600 por I2C (angulo y bits de estado del iman).'
    'src/sensors/as5600.h'          = 'Interfaz del driver AS5600.'
    'src/sensors/rom_calib.c'       = 'Calibracion del rango de movimiento del paciente y clasificacion dentro/limite/fuera.'
    'src/sensors/rom_calib.h'       = 'Interfaz de la calibracion del rango (ROM).'
    'Pruebas/banco/fase3_encoder/fase3_check.c'        = 'Banco de la Fase 3: verifica el AS5600 y la calibracion del rango. El motor queda deshabilitado.'
    'src/app/ui.h'                  = 'Interfaz de la UI minima (eventos de pulsador/serie) usada por el diag y la app.'
    'src/app/ui_basic.c'            = 'Implementacion de la UI basica (confirmar/cancelar/recalibrar).'
    'src/app/ui_basic.h'            = 'Interfaz de la UI basica.'

    'src/control/intent.c'          = 'Logica pura de intencion: XOR de los dos EMG para decidir flexion/extension/nada.'
    'src/control/intent.h'          = 'Interfaz de la intencion.'
    'src/control/limits.c'          = 'Logica pura de limites por angulo (zona de limite y corte).'
    'src/control/limits.h'          = 'Interfaz de limites.'
    'src/control/power.c'           = 'Logica pura de potencia: convierte intensidad EMG en duty aplicando rampas y topes.'
    'src/control/power.h'           = 'Interfaz de potencia.'

    'src/sensors/emg.c'             = 'Canal EMG: envolvente, deteccion de activo e intensidad a partir de la muestra.'
    'src/sensors/emg.h'             = 'Interfaz del canal EMG.'
    'src/sensors/emg_calib.c'       = 'Calibracion del EMG: reposo y contraccion maxima (MVC) por musculo.'
    'src/sensors/emg_calib.h'       = 'Interfaz de la calibracion EMG.'
    'Pruebas/banco/emg/emg_check.c'        = 'Banco del canal EMG (target emg_check): vuelca CSV de ambos canales. El motor queda deshabilitado.'

    'src/app/supervisor.c'          = 'Maquina de estados de seguridad: co-contraccion, limite, falla de encoder y rearme.'
    'src/app/supervisor.h'          = 'Interfaz del supervisor.'
    'src/app/angle_watch.c'         = 'Vigilancia del angulo y de la salud del encoder para el supervisor.'
    'src/app/angle_watch.h'         = 'Interfaz de la vigilancia de angulo.'
    'src/app/app.c'                 = 'Integracion del ciclo: lee sensores, corre control+supervisor, maneja el motor y patea el watchdog.'
    'src/app/app.h'                 = 'Interfaz del ciclo de aplicacion.'
    'src/app/telemetry.c'           = 'Telemetria por USB (esqueleto para reporte de estado).'
    'src/app/telemetry.h'           = 'Interfaz de telemetria.'
    'src/main.c'                    = 'Punto de entrada del firmware: inicializa el SDK y arranca el ciclo con UI y reporte USB.'

    'Pruebas/unitarias/CMakeLists.txt'          = 'Build de las pruebas de PC (Unity): define cada target test_<modulo> y sus fuentes.'
    'Pruebas/unitarias/unity/unity.c'           = 'Framework de pruebas Unity (implementacion).'
    'Pruebas/unitarias/unity/unity.h'           = 'Framework de pruebas Unity (API).'
    'Pruebas/unitarias/unity/unity_internals.h' = 'Framework de pruebas Unity (internos).'
    'Pruebas/unitarias/unity/.gitkeep'          = 'Marcador de carpeta.'
    'Pruebas/unitarias/mocks/mock_hal.h'        = 'Declaraciones comunes de los mocks del HAL para las pruebas de PC.'
    'Pruebas/unitarias/mocks/mock_hal_gpio.c'   = 'Mock del HAL de GPIO (registra enables y estado del driver).'
    'Pruebas/unitarias/mocks/mock_hal_i2c.c'    = 'Mock del HAL de I2C (simula registros del AS5600).'
    'Pruebas/unitarias/mocks/mock_hal_pwm.c'    = 'Mock del HAL de PWM (captura el duty aplicado a cada canal).'
    'Pruebas/unitarias/mocks/.gitkeep'          = 'Marcador de carpeta.'
    'Pruebas/unitarias/test_bts7960.c'          = 'Pruebas del driver BTS7960 (exclusion RPWM/LPWM, freno antes de invertir).'
    'Pruebas/unitarias/test_as5600.c'           = 'Pruebas del driver AS5600.'
    'Pruebas/unitarias/test_rom_calib.c'        = 'Pruebas de la calibracion del rango (ROM).'
    'Pruebas/unitarias/test_intent.c'           = 'Pruebas de la logica de intencion (XOR de EMG).'
    'Pruebas/unitarias/test_limits.c'           = 'Pruebas de la logica de limites por angulo.'
    'Pruebas/unitarias/test_power.c'            = 'Pruebas de la logica de potencia (rampas y topes de duty).'
    'Pruebas/unitarias/test_control_chain.c'    = 'Prueba de integracion de la cadena de control (intent -> limits -> power).'
    'Pruebas/unitarias/test_emg.c'              = 'Pruebas del canal EMG (envolvente, activo, picos y saturacion).'
    'Pruebas/unitarias/test_supervisor.c'       = 'Pruebas del supervisor: transiciones, rearme y fallas.'
    'Pruebas/unitarias/test_motor_output.c'     = 'Pruebas de la salida al motor (control+limites -> BTS7960 con mocks).'

    'tools/flash.sh'                = 'Script de carga del firmware a la placa.'
    'Pruebas/telemetry_plot.py'       = 'Graficado de la telemetria recibida por USB.'
    'Pruebas/unitarias/run_tests_wsl.ps1'       = 'Forma oficial de correr las pruebas de PC: compila y ejecuta Unity en WSL 2.'
    'tools/make_uf2_wsl.ps1'        = 'Genera el .uf2 del firmware usando WSL.'
    'tools/make_entregas.ps1'       = 'Este script: regenera los zips de entrega por fase y el indice.'
}

# ---------------------------------------------------------------------------
# Definicion de las fases (el orden se respeta en el indice)
# ---------------------------------------------------------------------------
$Fases = @(
    [ordered]@{
        Zip     = 'fase0_estructura.zip'
        Titulo  = 'Fase 0 - Estructura del proyecto'
        Tag     = 'fase-0'
        Que     = 'Esqueleto del repositorio: arbol de carpetas por capas, build del firmware, convenciones de codigo, tipos y constantes comunes, y la documentacion base (plan y hardware).'
        Pruebas = 'No tiene pruebas de PC propias. Se valida compilando el firmware: cmake -B build -G Ninja -DPICO_BOARD=pico2_w; cmake --build build'
        Commits = @('c38da4c - chore(fase-0): esqueleto de carpetas, build y convenciones')
        Files   = @(
            'CMakeLists.txt','CLAUDE.md','README.md','pico_sdk_import.cmake','.clang-format',
            'src/common/config.h','src/common/redux_types.h','src/common/util.h',
            'docs/PLAN.md','docs/hardware.md','docs/decisions/.gitkeep'
        )
    },
    [ordered]@{
        Zip     = 'fase1_hal.zip'
        Titulo  = 'Fase 1 - HAL (capa de abstraccion de hardware)'
        Tag     = 'fase-1'
        Que     = 'Capa HAL completa sobre el Pico SDK: GPIO, ADC (EMG), I2C (encoder), PWM, tiempo con timeout, watchdog y pulsador. Incluye el banco de bring-up fase1_check que ejercita el HAL sin activar el motor.'
        Pruebas = 'No tiene pruebas de PC (el HAL toca hardware). Se verifica en la placa con el target fase1_check.'
        Commits = @(
            '0cd6f40 - feat(hal): implementar config, tipos y HAL de la Fase 1',
            'fcb2f3c - feat(hal): agregar hal_gpio_motor_enable para el BTS7960 (retoque)',
            'e8cb6fd - fix(hal): pinout definitivo de la PCB (retoque de HAL)'
        )
        Files   = @(
            'src/hal/board.h',
            'src/hal/hal_adc.c','src/hal/hal_adc.h',
            'src/hal/hal_button.c','src/hal/hal_button.h',
            'src/hal/hal_gpio.c','src/hal/hal_gpio.h',
            'src/hal/hal_i2c.c','src/hal/hal_i2c.h',
            'src/hal/hal_pwm.c','src/hal/hal_pwm.h',
            'src/hal/hal_time.c','src/hal/hal_time.h',
            'src/hal/hal_wdt.c','src/hal/hal_wdt.h',
            'Pruebas/banco/fase1_hal/fase1_check.c'
        )
    },
    [ordered]@{
        Zip     = 'fase2_motor.zip'
        Titulo  = 'Fase 2 - Motor (driver BTS7960)'
        Tag     = 'fase-2'
        Que     = 'Driver del puente H BTS7960 con sus reglas de seguridad (RPWM y LPWM nunca con duty a la vez, freno a 0 antes de invertir), el HAL de PWM que usa, el banco fase2_check para probar el motor sin carga por comandos serie, y las pruebas de PC del driver con sus mocks.'
        Pruebas = 'En WSL: Pruebas/unitarias/run_tests_wsl.ps1 (compila y corre todas). Solo esta fase: ctest --test-dir build-tests-wsl -R test_bts7960'
        Commits = @(
            '1de50de - feat(bts7960): driver del puente H con pruebas en PC',
            '578fe3c - feat(diag): programa de banco fase2_check del BTS7960',
            'fcb2f3c - feat(hal): hal_gpio_motor_enable para el BTS7960',
            'e8cb6fd - fix(hal): pinout definitivo de la PCB (enables fijos)'
        )
        Files   = @(
            'src/actuators/bts7960.c','src/actuators/bts7960.h',
            'src/hal/hal_pwm.c','src/hal/hal_pwm.h',
            'Pruebas/banco/fase2_motor/fase2_check.c',
            'Pruebas/unitarias/test_bts7960.c',
            'Pruebas/unitarias/mocks/mock_hal.h','Pruebas/unitarias/mocks/mock_hal_gpio.c','Pruebas/unitarias/mocks/mock_hal_pwm.c'
        )
    },
    [ordered]@{
        Zip     = 'fase3_encoder.zip'
        Titulo  = 'Fase 3 - Encoder AS5600 y calibracion del rango'
        Tag     = 'fase-3'
        Que     = 'Driver del encoder magnetico AS5600 por I2C, la calibracion del rango de movimiento del paciente (rom_calib), la UI basica que usa el banco, el banco fase3_check y las pruebas de PC del encoder y de la calibracion.'
        Pruebas = 'En WSL: Pruebas/unitarias/run_tests_wsl.ps1. Solo esta fase: ctest --test-dir build-tests-wsl -R "test_as5600|test_rom_calib"'
        Commits = @(
            '1378570 - feat(sensors): driver AS5600 y calibracion del rango del paciente (Fase 3)',
            'd9f967b - feat(diag): mostrar la lectura cruda del AS5600 durante la calibracion'
        )
        Files   = @(
            'src/sensors/as5600.c','src/sensors/as5600.h',
            'src/sensors/rom_calib.c','src/sensors/rom_calib.h',
            'Pruebas/banco/fase3_encoder/fase3_check.c',
            'src/app/ui.h','src/app/ui_basic.c','src/app/ui_basic.h',
            'Pruebas/unitarias/test_as5600.c','Pruebas/unitarias/test_rom_calib.c',
            'Pruebas/unitarias/mocks/mock_hal.h','Pruebas/unitarias/mocks/mock_hal_i2c.c'
        )
    },
    [ordered]@{
        Zip     = 'fase4_control.zip'
        Titulo  = 'Fase 4 - Control (logica pura)'
        Tag     = 'fase-4'
        Que     = 'Logica de control pura y probable en PC: intencion (XOR de los dos EMG), limites por angulo y potencia (intensidad EMG -> duty con rampas y topes). Sin dependencias del hardware; incluye la prueba de integracion de toda la cadena.'
        Pruebas = 'En WSL: Pruebas/unitarias/run_tests_wsl.ps1. Solo esta fase: ctest --test-dir build-tests-wsl -R "test_intent|test_limits|test_power|test_control_chain"'
        Commits = @('661e69f - feat(control): logica pura de intencion, limites y potencia (numerado Fase 5 en el repo)')
        Files   = @(
            'src/control/intent.c','src/control/intent.h',
            'src/control/limits.c','src/control/limits.h',
            'src/control/power.c','src/control/power.h',
            'Pruebas/unitarias/test_intent.c','Pruebas/unitarias/test_limits.c','Pruebas/unitarias/test_power.c','Pruebas/unitarias/test_control_chain.c'
        )
    },
    [ordered]@{
        Zip     = 'fase5_emg.zip'
        Titulo  = 'Fase 5 - EMG (canal y calibracion)'
        Tag     = 'fase-5'
        Que     = 'Procesamiento del canal EMG (envolvente, deteccion de activo e intensidad) y su calibracion por musculo (reposo y contraccion maxima). Incluye el banco emg_check (antes fase4_check en el repo) y las pruebas de PC del canal.'
        Pruebas = 'En WSL: Pruebas/unitarias/run_tests_wsl.ps1. Solo esta fase: ctest --test-dir build-tests-wsl -R test_emg'
        Commits = @(
            'bfffe3c - feat(emg): canal EMG, calibracion y banco fase4_check (numerado Fase 4 en el repo)',
            '8c6d9a6 - fix(emg): corregir test del pico corto y cubrir artefacto por saturacion'
        )
        Files   = @(
            'src/sensors/emg.c','src/sensors/emg.h',
            'src/sensors/emg_calib.c','src/sensors/emg_calib.h',
            'Pruebas/banco/emg/emg_check.c',
            'Pruebas/unitarias/test_emg.c'
        )
    },
    [ordered]@{
        Zip     = 'fase6_supervisor.zip'
        Titulo  = 'Fase 6 - Supervisor, app y firmware integrado'
        Tag     = 'fase-6'
        Que     = 'Maquina de estados de seguridad (supervisor) con vigilancia de angulo, el ciclo de aplicacion que integra sensores + control + supervisor + motor + watchdog, el main del firmware, la documentacion de la maquina de estados y las pruebas del supervisor y de la salida al motor.'
        Pruebas = 'En WSL: Pruebas/unitarias/run_tests_wsl.ps1. Solo esta fase: ctest --test-dir build-tests-wsl -R "test_supervisor|test_motor_output"'
        Commits = @(
            '2a75e87 - feat(supervisor): maquina de estados de seguridad',
            '8cb479c - feat(app): integracion del ciclo y watchdog',
            '0ebb316 - feat(app): firmware con ui y reporte USB',
            '688e566 - test(supervisor): transiciones, rearme y fallas',
            'cc57eeb - test(diag): comando hang para probar watchdog',
            '939dac2 - docs(plan): actualizar plan con fase 6',
            'a9cddb4 - fix(app): mostrar raw del encoder en el reporte',
            '80da459 - fix(app): inicializar el PWM del motor y reportar el duty aplicado'
        )
        Files   = @(
            'src/app/supervisor.c','src/app/supervisor.h',
            'src/app/angle_watch.c','src/app/angle_watch.h',
            'src/app/app.c','src/app/app.h',
            'src/app/telemetry.c','src/app/telemetry.h',
            'src/main.c',
            'docs/state_machine.md',
            'Pruebas/unitarias/test_supervisor.c','Pruebas/unitarias/test_motor_output.c',
            'Pruebas/unitarias/mocks/mock_hal.h','Pruebas/unitarias/mocks/mock_hal_gpio.c','Pruebas/unitarias/mocks/mock_hal_pwm.c'
        )
    },
    [ordered]@{
        Zip     = 'herramientas.zip'
        Titulo  = 'Herramientas y framework de pruebas'
        Tag     = ''
        Que     = 'No es una fase: reune los scripts de tools/ (pruebas y uf2 por WSL, carga, graficado y este generador de entregas) y el framework de pruebas completo (Unity, todos los mocks del HAL y el CMake de pruebas). Es lo transversal que usan todas las fases.'
        Pruebas = 'Es el framework en si. Forma oficial de correr las pruebas: Pruebas/unitarias/run_tests_wsl.ps1 (compila y corre Unity en WSL 2, build en build-tests-wsl/).'
        Commits = @(
            'c38da4c - chore(fase-0): tools/ y tests/ iniciales',
            '1de50de - feat(bts7960): Unity y primeros mocks',
            '1378570 - feat(sensors): mock de I2C',
            '35c0ea2 - chore(tests): correr las pruebas de PC en WSL',
            'c53d55a - chore(tools): generar uf2 por WSL'
        )
        Files   = @(
            'tools/flash.sh','Pruebas/telemetry_plot.py','Pruebas/unitarias/run_tests_wsl.ps1','tools/make_uf2_wsl.ps1','tools/make_entregas.ps1',
            'Pruebas/unitarias/CMakeLists.txt',
            'Pruebas/unitarias/unity/unity.c','Pruebas/unitarias/unity/unity.h','Pruebas/unitarias/unity/unity_internals.h','Pruebas/unitarias/unity/.gitkeep',
            'Pruebas/unitarias/mocks/mock_hal.h','Pruebas/unitarias/mocks/mock_hal_gpio.c','Pruebas/unitarias/mocks/mock_hal_i2c.c','Pruebas/unitarias/mocks/mock_hal_pwm.c','Pruebas/unitarias/mocks/.gitkeep'
        )
    }
)

# ---------------------------------------------------------------------------
# Generacion
# ---------------------------------------------------------------------------
function New-LeemeText {
    param($Fase)
    $sb = New-Object System.Text.StringBuilder
    [void]$sb.AppendLine("# $($Fase.Titulo)")
    [void]$sb.AppendLine("")
    [void]$sb.AppendLine("## Que hace")
    [void]$sb.AppendLine($Fase.Que)
    [void]$sb.AppendLine("")
    [void]$sb.AppendLine("## Archivos")
    foreach ($f in $Fase.Files) {
        $d = $Desc[$f]
        if (-not $d) { $d = '(sin descripcion)' }
        [void]$sb.AppendLine("- ``$f`` - $d")
    }
    [void]$sb.AppendLine("")
    [void]$sb.AppendLine("## Como correr sus pruebas")
    [void]$sb.AppendLine($Fase.Pruebas)
    [void]$sb.AppendLine("")
    [void]$sb.AppendLine("## Commits de git que la componen")
    foreach ($c in $Fase.Commits) {
        [void]$sb.AppendLine("- $c")
    }
    if ($Fase.Tag) {
        [void]$sb.AppendLine("")
        [void]$sb.AppendLine("Tag de referencia: ``$($Fase.Tag)``")
    }
    return $sb.ToString()
}

# Carpeta de salida limpia
if (Test-Path $StagingRoot) { Remove-Item -Recurse -Force $StagingRoot }
New-Item -ItemType Directory -Force -Path $EntregasDir | Out-Null
# Borrar zips viejos para que no queden entregas obsoletas
Get-ChildItem -Path $EntregasDir -Filter '*.zip' -File -ErrorAction SilentlyContinue | Remove-Item -Force

$missing = @()

foreach ($fase in $Fases) {
    $stage = Join-Path $StagingRoot ($fase.Zip -replace '\.zip$','')
    New-Item -ItemType Directory -Force -Path $stage | Out-Null

    foreach ($rel in $fase.Files) {
        $src = Join-Path $RepoRoot $rel
        if (-not (Test-Path $src)) {
            $missing += "$($fase.Zip): falta $rel"
            continue
        }
        $dst = Join-Path $stage $rel
        $dstDir = Split-Path -Parent $dst
        if (-not (Test-Path $dstDir)) { New-Item -ItemType Directory -Force -Path $dstDir | Out-Null }
        Copy-Item -LiteralPath $src -Destination $dst -Force
    }

    # LEEME.md en la raiz del zip
    $leeme = New-LeemeText -Fase $fase
    Set-Content -Path (Join-Path $stage 'LEEME.md') -Value $leeme -Encoding UTF8

    $zipPath = Join-Path $EntregasDir $fase.Zip
    if (Test-Path $zipPath) { Remove-Item -Force $zipPath }
    Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zipPath -Force
    Write-Host ("OK  {0}" -f $fase.Zip)
}

# Limpieza del staging
Remove-Item -Recurse -Force $StagingRoot

# ---------------------------------------------------------------------------
# Indice entregas/README.md
# ---------------------------------------------------------------------------
$r = New-Object System.Text.StringBuilder
[void]$r.AppendLine("# Entregas por fase - firmware REDUX")
[void]$r.AppendLine("")
[void]$r.AppendLine("Exoesqueleto de codo. Raspberry Pi Pico 2 W (RP2350), C11, Pico SDK 2.x.")
[void]$r.AppendLine("")
[void]$r.AppendLine("Cada zip contiene el codigo, las pruebas y el diagnostico de una fase, mas un ``LEEME.md`` que explica que hace la fase, lista sus archivos, como correr sus pruebas y los commits que la componen. Los zips se regeneran con ``tools/make_entregas.ps1`` y no se versionan (estan en .gitignore); este indice y el script si.")
[void]$r.AppendLine("")
[void]$r.AppendLine("Nota: la numeracion interna del repo cruza control y EMG (el commit de control dice 'Fase 5', el de EMG 'Fase 4', y el banco ahora se llama emg_check -antes fase4_check-). Estas entregas se numeran por contenido: fase4 = control, fase5 = EMG.")
[void]$r.AppendLine("")
[void]$r.AppendLine("## Indice")
[void]$r.AppendLine("")
[void]$r.AppendLine("| Fase | Zip | Tag | Contenido |")
[void]$r.AppendLine("|------|-----|-----|-----------|")
foreach ($fase in $Fases) {
    $tag = if ($fase.Tag) { "``$($fase.Tag)``" } else { '-' }
    $resumen = ($fase.Titulo -replace '^Fase \d+ - ','' -replace '^','')
    [void]$r.AppendLine("| $($fase.Titulo) | ``$($fase.Zip)`` | $tag | $resumen |")
}
[void]$r.AppendLine("")
foreach ($fase in $Fases) {
    [void]$r.AppendLine("### $($fase.Titulo)")
    [void]$r.AppendLine("")
    [void]$r.AppendLine("- Zip: ``$($fase.Zip)``")
    if ($fase.Tag) { [void]$r.AppendLine("- Tag: ``$($fase.Tag)``") }
    [void]$r.AppendLine("- $($fase.Que)")
    [void]$r.AppendLine("- Commits:")
    foreach ($c in $fase.Commits) { [void]$r.AppendLine("  - $c") }
    [void]$r.AppendLine("")
}
[void]$r.AppendLine("## Como regenerar los zips")
[void]$r.AppendLine("")
[void]$r.AppendLine('```powershell')
[void]$r.AppendLine("powershell -ExecutionPolicy Bypass -File tools\make_entregas.ps1")
[void]$r.AppendLine('```')
Set-Content -Path (Join-Path $EntregasDir 'README.md') -Value $r.ToString() -Encoding UTF8
Write-Host "OK  entregas/README.md"

if ($missing.Count -gt 0) {
    Write-Warning "Archivos no encontrados:"
    $missing | ForEach-Object { Write-Warning "  $_" }
    throw "Faltan archivos; revisar el reparto."
}

Write-Host ""
Write-Host "Entregas generadas en: $EntregasDir"
