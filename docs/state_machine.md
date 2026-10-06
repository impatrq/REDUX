<!-- state_machine.md · Estados y transiciones de la máquina de seguridad. Fase 6. -->

# Máquina de estados del supervisor

El supervisor (`src/app/supervisor.c`) es lógica pura: una llamada por tick de 1 ms
(`supervisor_step`) recibe las entradas del ciclo y decide el estado y si el motor puede
moverse. **El motor solo recibe PWM en `ST_RUN`** (`motor_allowed == (state == ST_RUN)`,
invariante verificada en las pruebas). Cualquier falla lleva a `ST_FAULT` con corte
inmediato en el mismo tick.

## Entradas por tick (`supervisor_in_t`)

| Campo | Significado |
| --- | --- |
| `enc_fault` | Fallas del encoder de este tick (máscara): `FAULT_I2C`, `FAULT_MAGNET`, `FAULT_ANGLE_JUMP`. `app` solo evalúa el salto de ángulo en `ST_ARMING`/`ST_RUN`. |
| `calib_done` / `calib_failed` | La calibración activa en el estado actual terminó bien / falló. |
| `emg_rest` | Ambos músculos en reposo (requisito para pasar a `ST_RUN`). |
| `emg_saturated` | Algún canal de EMG saturado (falla en marcha). |
| `cycle_overrun` | El ciclo excedió `CTRL_TICK_US` (medición de `app_tick` o ticks perdidos). |
| `ui_event` | Evento de la interfaz: `UI_EV_CONFIRM`, `UI_EV_CANCEL`, `UI_EV_RECALIBRATE`. |

## Estados

`ST_BOOT → ST_CHECK_ENC → ST_CALIB_ROM → ST_CALIB_REST → ST_CALIB_MVC → ST_ARMING → ST_RUN`,
más `ST_FAULT`. La calibración de rango (ROM, Fase 3) va antes de las dos tomas de EMG
(reposo y contracción máxima, Fase 4).

## Transiciones

```
BOOT        --UI_EV_CONFIRM--------------------> CHECK_ENC
CHECK_ENC   --enc_fault == 0-------------------> CALIB_ROM        (espera, motor off, si hay falla)
CALIB_ROM   --calib_done----------------------> CALIB_REST
CALIB_REST  --calib_done----------------------> CALIB_MVC
CALIB_MVC   --calib_done----------------------> ARMING
ARMING      --emg_rest------------------------> RUN
RUN         --(co-contracción / límite)-------> RUN  (no cambia de estado; corta la cadena)
```

Orden de decisión en cada estado: **1) fallas de seguridad** (van a `ST_FAULT` en el
mismo tick), **2) eventos de UI**, **3) progresión normal**.

### Fallas → `ST_FAULT` (corte inmediato)

- `CALIB_ROM`, `CALIB_REST`, `CALIB_MVC`: `enc_fault != 0` → `FAULT`.
- `ARMING`: `enc_fault != 0` o `CTRL_OVERRUN_MAX` sobrepasos seguidos → `FAULT`.
- `RUN`: `enc_fault != 0`, overrun, o `emg_saturated` (→ `FAULT_EMG_SAT`) → `FAULT`.
- `CHECK_ENC` no falla: espera con el motor apagado hasta que el encoder esté sano.
- Co-contracción y límite alcanzado **no** son fallas del supervisor: el corte lo hace
  la cadena de control (`hard_stop`), el supervisor sigue en `ST_RUN`.

### Eventos de UI

- `UI_EV_RECALIBRATE` desde `CALIB_*`, `ARMING` o `RUN` → `CALIB_ROM` (motor apagado).
- `UI_EV_CANCEL` durante `CALIB_*` → `CHECK_ENC`, sin guardar la calibración en curso.
- `UI_EV_CANCEL` en `RUN` → `ARMING`, con el motor apagado. En `ARMING` no hace nada
  (ya está apagado).
- `UI_EV_CONFIRM` en `BOOT` → `CHECK_ENC` (arranque pedido por el usuario).

### Rearme desde `ST_FAULT`

Se cuentan lecturas estables (`enc_fault == 0 && !emg_saturated && !cycle_overrun`);
cualquier recaída reinicia el contador. Tras `REARM_OK_TICKS` estables, el destino
depende de la causa de la falla:

- La falla involucró al encoder (algún bit de `FAULT_ENC_MASK` = `I2C | MAGNET |
  ANGLE_JUMP`) → **`CALIB_ROM`**: el rango ya no es confiable y hay que recalibrar.
- Cualquier otra falla (`FAULT_OVERRUN`, `FAULT_EMG_SAT`) → **`ARMING`**, conservando la
  calibración. Desde `ARMING` solo se vuelve a `RUN` con ambos músculos en reposo.

## Reglas de seguridad asociadas (ver `docs/PLAN.md`)

1. `motor_allowed` solo true en `ST_RUN`; `app` llama `bts7960_stop()` en cualquier otro
   estado. Los enables del driver van fijos en alto en la PCB: el corte es siempre
   RPWM = LPWM = 0.
2. El detector de salto de ángulo reinicia su referencia al entrar a `ARMING` y a `RUN`,
   para que los movimientos hechos en `FAULT` o durante la calibración no disparen un
   falso `FAULT_ANGLE_JUMP`. `ANGLE_JUMP_MAX_DEG = 5°` (≈ 5000 °/s, no fisiológico).
3. El watchdog (`WDT_TIMEOUT_MS`) se habilita **una sola vez**, al entrar por primera vez
   a `ARMING` (después de calibrar), y se alimenta en cada tick desde ahí. Volver a
   `ARMING` tras un rearme no lo rehabilita.
