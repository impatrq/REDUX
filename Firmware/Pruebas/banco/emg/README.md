# Banco EMG — canal y calibración

Programa `emg_check` (antes `fase4_check` en el repo): verifica el canal EMG y su
calibración. Sirve con el EMG real o con un **potenciómetro** en GP26/GP27 simulando la
envolvente. **El driver del motor queda deshabilitado siempre.**

## Qué prueba
- El procesamiento del canal EMG: envolvente (filtro exponencial), detección de **activo**
  (con histéresis y tiempo mínimo) e **intensidad** normalizada.
- El sobremuestreo de `EMG_OVERSAMPLE` lecturas por canal y por tick (lo mismo que hace
  `app_tick` en el firmware).
- La calibración por músculo: **reposo** (media y desvío) y **contracción máxima (MVC)**.

Vuelca un CSV cada `FASE4_REPORT_MS` (20 ms) con, por canal, envolvente, activo,
intensidad y saturación:
`t_ms,env_b,act_b,int_b,sat_b,env_t,act_t,int_t,sat_t`.

## Conexiones (protoboard)
- **EMG bíceps** en GP26 / ADC0 y **tríceps** en GP27 / ADC1. La señal real llega ya
  rectificada (onda completa), sin suavizar, 0–3 V, con 2k2 en serie; su masa va a
  AGND / GND2.
- Para probar sin electrodos, un **potenciómetro** en cada entrada simula la envolvente.
- Motor deshabilitado; R_EN/L_EN a +5 V. Ver [`docs/hardware.md`](../../../docs/hardware.md).

## Cómo cargarlo
1. `cmake -B build -G Ninja -DPICO_BOARD=pico2_w && cmake --build build`
2. `tools\make_uf2_wsl.ps1 -Elf build\emg_check.elf -Uf2 build\emg_check.uf2`
3. BOOTSEL y copiar `build\emg_check.uf2` a `RPI-RP2` (o `tools/flash.sh`).
4. Abrir el monitor serie USB. Para graficar el CSV en vivo se puede usar
   [`Pruebas/telemetry_plot.py`](../../telemetry_plot.py).

## Comandos serie
| Comando | Efecto |
| --- | --- |
| `rest` | Calibra el reposo de ambos canales (ambos músculos relajados) |
| `mvc b` | Calibra la contracción máxima del bíceps |
| `mvc t` | Calibra la contracción máxima del tríceps |
