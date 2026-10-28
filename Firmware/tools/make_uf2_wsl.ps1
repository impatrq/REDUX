# make_uf2_wsl.ps1 · Genera el .uf2 del firmware usando un picotool nativo de Linux
# compilado en WSL 2, sin tocar la configuracion de Windows.
#
# Por que: en esta maquina Device Guard / Smart App Control bloquea picotool.exe (el
# paso de empaquetado del build de CMake en Windows), aunque el enlace del .elf se
# complete. WSL no esta sujeto a ese bloqueo. Este script compila picotool una sola vez
# dentro de WSL (en ~/.local, NO en el repo) usando como PICO_SDK_PATH el SDK ya
# instalado en Windows, y convierte el .elf (que si produce el build de Windows) a .uf2.
#
# Uso (desde cualquier carpeta):
#   powershell -ExecutionPolicy Bypass -File tools\make_uf2_wsl.ps1
#   tools\make_uf2_wsl.ps1 -Elf build\fase2_check.elf -Uf2 build\fase2_check.uf2
#   tools\make_uf2_wsl.ps1 -Rebuild     # fuerza recompilar picotool
#
# Requisitos en WSL: gcc/g++, cmake, git (libusb NO hace falta: uf2 convert e info
# sobre archivos funcionan sin soporte USB). Deja el binario en ~/.local/bin/picotool.

[CmdletBinding()]
param(
    [string]$Elf = 'build/redux-firmware.elf',
    [string]$Uf2 = 'build/redux-firmware.uf2',
    [string]$Family = 'rp2350-arm-s',
    [switch]$Rebuild
)

$ErrorActionPreference = 'Stop'

# Traduce una ruta Windows (C:\a\b) a WSL (/mnt/c/a/b).
function To-Wsl([string]$winPath) {
    $full = (Resolve-Path $winPath -ErrorAction SilentlyContinue)
    if ($full) { $winPath = $full.Path }
    if ($winPath -notmatch '^([A-Za-z]):\\(.*)$') {
        Write-Error "Ruta inesperada: $winPath"; exit 1
    }
    $drive = $Matches[1].ToLower()
    $rest = $Matches[2] -replace '\\', '/'
    return "/mnt/$drive/$rest"
}

$repoRoot = (Resolve-Path (Split-Path -Parent $PSScriptRoot)).Path
$wslRoot = To-Wsl $repoRoot
# El SDK de Windows (mismo que usa el build de CMake) como PICO_SDK_PATH para compilar
# picotool. Se asume la instalacion estandar de la extension Pico en el perfil del usuario.
$sdkWsl = To-Wsl (Join-Path $env:USERPROFILE '.pico-sdk\sdk\2.3.1')

# Rutas relativas al repo (el .elf/.uf2 viven en build/).
$elfRel = ($Elf -replace '\\', '/')
$uf2Rel = ($Uf2 -replace '\\', '/')
$rebuildFlag = if ($Rebuild) { '1' } else { '0' }

Write-Host "Repo (WSL):  $wslRoot" -ForegroundColor Cyan
Write-Host "SDK  (WSL):  $sdkWsl" -ForegroundColor Cyan
Write-Host "ELF -> UF2:  $elfRel -> $uf2Rel  (familia $Family)" -ForegroundColor Cyan

$bash = @"
set -e
export PICO_SDK_PATH='$sdkWsl'
PT="`$HOME/.local/bin/picotool"
if [ '$rebuildFlag' = '1' ]; then rm -f "`$PT"; fi
if [ ! -x "`$PT" ]; then
    echo '== picotool no encontrado: compilando en WSL (una sola vez) =='
    if [ ! -x "`$HOME/picotool-src/build/picotool" ]; then
        rm -rf "`$HOME/picotool-src"
        git clone --depth 1 --branch 2.3.1 https://github.com/raspberrypi/picotool.git "`$HOME/picotool-src"
        cmake -S "`$HOME/picotool-src" -B "`$HOME/picotool-src/build" -DCMAKE_BUILD_TYPE=Release
        cmake --build "`$HOME/picotool-src/build" -j4
    fi
    mkdir -p "`$HOME/.local/bin"
    cp "`$HOME/picotool-src/build/picotool" "`$PT"
fi
cd '$wslRoot'
echo '== convert =='
"`$PT" uf2 convert '$elfRel' '$uf2Rel' --family '$Family'
echo '== info =='
"`$PT" info '$uf2Rel'
"@

& wsl.exe -e bash -lc $bash
exit $LASTEXITCODE
