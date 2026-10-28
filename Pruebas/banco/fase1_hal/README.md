# Banco Fase 1 — HAL (bring-up)

Programa `fase1_check`: ejercita la capa de abstracción de hardware (HAL) sobre la
Pico 2 W e imprime por USB. **El motor nunca se activa.** Sirve para verificar que los
periféricos básicos responden antes de montar el resto.

## Qué prueba
- Tick de 1 ms del `hal_time`: imprime los ticks por segundo (deben ser ~1000).
- Pulsador con antirrebote (`hal_button`): imprime `pulsador` en cada pulsación.
- Los dos canales de ADC (`hal_adc`): vuelca las lecturas crudas de bíceps y tríceps.
- PWM (`hal_pwm`): varía el brillo del LED de estado con una rampa triangular.

## Conexiones (protoboard)
- **LED de estado** en GP2 (a LED + resistencia a GND, activo en alto).
- **Pulsador** entre GP3 y GND (pull-up interno, activo en bajo).
- **EMG bíceps** en GP26 / ADC0 y **tríceps** en GP27 / ADC1 (0–3 V). Para la prueba
  alcanza con un potenciómetro en cada entrada; con las entradas al aire el valor flota.
- **R_EN / L_EN del BTS7960 a +5 V** del lado del motor (el firmware no los maneja).

Pinout completo en [`docs/hardware.md`](../../../docs/hardware.md).

## Cómo cargarlo
1. Compilar (desde la raíz del repo): `cmake -B build -G Ninja -DPICO_BOARD=pico2_w && cmake --build build`.
2. Generar el `.uf2` del banco (picotool por WSL):
   `tools\make_uf2_wsl.ps1 -Elf build\fase1_check.elf -Uf2 build\fase1_check.uf2`
3. Poner la Pico en modo BOOTSEL y copiar `build\fase1_check.uf2` a la unidad `RPI-RP2`
   (o `tools/flash.sh`).
4. Abrir el monitor serie USB a cualquier baudrate.

## Comandos serie
Ninguno: el banco solo imprime. Para probar el pulsador, presionarlo; para los ADC,
variar la tensión en GP26/GP27.
