# Banco Fase 2 — Motor (driver BTS7960)

Programa `fase2_check`: maneja el puente H BTS7960 con el **motor sin carga**, por
comandos del monitor serie. Verifica las reglas de seguridad del driver antes de
integrarlo al firmware.

## Qué prueba
- Que RPWM y LPWM **nunca** tengan duty > 0 a la vez.
- Que al invertir el sentido **primero frena** y espera `FASE2_REVERSE_BRAKE_MS` (200 ms).
- **Hombre muerto**: si no llega ningún comando en `FASE2_DEADMAN_MS` (5 s), frena solo.
- El duty se limita a `FASE2_DUTY_MAX_PCT` (50 %) como tope del banco.

Imprime el estado (`driver`, `dir`, `duty`) tras cada comando.

## Conexiones (protoboard)
- **RPWM** (flexión) en GP8 y **LPWM** (extensión) en GP9, a 20 kHz. Si hay
  optoacopladores 6N137 a mano, intercalarlos (no invierten) reproduce el estado seguro
  al reset.
- **R_EN / L_EN del BTS7960 a +5 V** del lado del motor (siempre habilitados; el firmware
  no los maneja). GP6/GP7 quedan sin conectar.
- Motor **sin carga** y alimentación de potencia en GND1. Ver [`docs/hardware.md`](../../../docs/hardware.md).

## Cómo cargarlo
1. `cmake -B build -G Ninja -DPICO_BOARD=pico2_w && cmake --build build`
2. `tools\make_uf2_wsl.ps1 -Elf build\fase2_check.elf -Uf2 build\fase2_check.uf2`
3. BOOTSEL y copiar `build\fase2_check.uf2` a `RPI-RP2` (o `tools/flash.sh`).
4. Abrir el monitor serie USB.

## Comandos serie
| Comando | Efecto |
| --- | --- |
| `f N` | Flexiona al N % (RPWM), con N ≤ 50 |
| `e N` | Extiende al N % (LPWM), con N ≤ 50 |
| `s` | Frena (duty 0) |
| `en 1` / `en 0` | Habilita / deshabilita el driver |
