# Banco Fase 3 — Encoder AS5600 y calibración del rango

Programa `fase3_check`: verifica el encoder magnético AS5600 por I2C y la calibración
del rango de movimiento del paciente (ROM). **El driver del motor queda deshabilitado
siempre** (solo se usa para dejarlo en estado seguro).

## Qué prueba
- Lectura del AS5600: ángulo crudo (RAW), grados y los bits del imán **MD/ML/MH**
  (detectado / muy débil / muy fuerte).
- Calibración del rango: toma de extremos, validación de recorrido mínimo/máximo y
  dispersión, y clasificación en **DENTRO / ZONA DE LÍMITE / FUERA** respecto de `rom_deg`.

En modo `raw` imprime RAW, grados y MD/ML/MH cada 200 ms. Tras calibrar imprime θ,
`rom_deg` y la zona cada 200 ms.

## Conexiones (protoboard)
- **AS5600**: SDA en GP4, SCL en GP5 (I2C0 a 400 kHz, dirección 0x36), alimentado en GND2.
  Pin **DIR del AS5600 a GND2**.
- **Imán** montado rígido a 0.5–3 mm del encoder (el banco chequea MD/ML/MH).
- **Pulsador** en GP3 para confirmar la calibración (o el comando `ok`).
- Motor deshabilitado; R_EN/L_EN a +5 V igual que en las otras fases. Ver
  [`docs/hardware.md`](../../../docs/hardware.md).

## Cómo cargarlo
1. `cmake -B build -G Ninja -DPICO_BOARD=pico2_w && cmake --build build`
2. `tools\make_uf2_wsl.ps1 -Elf build\fase3_check.elf -Uf2 build\fase3_check.uf2`
3. BOOTSEL y copiar `build\fase3_check.uf2` a `RPI-RP2` (o `tools/flash.sh`).
4. Abrir el monitor serie USB.

## Comandos serie
| Comando | Efecto |
| --- | --- |
| `cal` | Corre la calibración del rango (confirmar con el pulsador u `ok`) |
| `raw` | Vuelve al modo de lectura cruda |
| `ok` / `cancel` / `recal` | Eventos de la interfaz durante la calibración |
