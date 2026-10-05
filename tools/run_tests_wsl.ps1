# run_tests_wsl.ps1 · Compila y corre las pruebas de PC (Unity) dentro de WSL 2.
#
# En esta maquina Windows, Smart App Control (Enforced) bloquea los .exe de prueba
# recien enlazados por MinGW, asi que las pruebas se corren en WSL (Ubuntu: gcc, cmake,
# ninja). Usa una carpeta de build separada (build-tests-wsl) para no pisar la de
# Windows (build-tests). Esta es la forma oficial de correr las pruebas (ver CLAUDE.md).
#
# Uso (desde cualquier carpeta):
#   powershell -ExecutionPolicy Bypass -File tools\run_tests_wsl.ps1
#   tools\run_tests_wsl.ps1 -Clean     # borra build-tests-wsl antes de configurar
#
# Devuelve el codigo de salida de ctest (0 = todas pasaron).

[CmdletBinding()]
param(
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'

# Raiz del repo = carpeta padre de tools/.
$repoRoot = (Resolve-Path (Split-Path -Parent $PSScriptRoot)).Path

# Traduce C:\Users\... a /mnt/c/Users/... (se hace aca y no con wslpath porque al pasar
# la ruta por wsl.exe se pierden las barras invertidas).
if ($repoRoot -notmatch '^([A-Za-z]):\\(.*)$') {
    Write-Error "Ruta de repo inesperada: $repoRoot"
    exit 1
}
$drive = $Matches[1].ToLower()
$rest = $Matches[2] -replace '\\', '/'
$wslRoot = "/mnt/$drive/$rest"

Write-Host "Repo (WSL): $wslRoot" -ForegroundColor Cyan
Write-Host "Corriendo pruebas en WSL..." -ForegroundColor Cyan

# Comando de build+test dentro de WSL. -lc carga el perfil para tener gcc/cmake/ninja
# en el PATH. set -e corta ante el primer error de configuracion/compilacion.
$cleanCmd = if ($Clean) { 'rm -rf build-tests-wsl;' } else { '' }
$bash = @"
set -e
cd '$wslRoot'
$cleanCmd
cmake -B build-tests-wsl -S tests -G Ninja
cmake --build build-tests-wsl
ctest --test-dir build-tests-wsl --output-on-failure
"@

& wsl.exe -e bash -lc $bash
exit $LASTEXITCODE
