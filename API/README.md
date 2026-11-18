# API de REDUX

API en **Flask + SQLAlchemy + SQLite** que recibe las sesiones del exoesqueleto
(desde la Pico) y sirve las páginas de datos de [`../Web_datos`](../Web_datos).
Es una sola app Flask: expone la API y además entrega las páginas web.

## Puesta en marcha

```bash
cd API
python -m venv .venv            # opcional
# Instalar dependencias
pip install -r requirements.txt

# Configurar las claves
cp .env.example .env            # y editar REDUX_API_KEY y SECRET_KEY
```

> El `.env` **no se versiona**. Si no existe, se usan valores de desarrollo.

La base se crea sola en `instance/redux.db` al arrancar (está en `.gitignore`).

## Correr el servidor

```bash
flask --app app run            # http://localhost:5000
# o
python app.py                  # host 0.0.0.0, debug apagado
```

`debug` está **apagado** por defecto. Para desarrollo: `FLASK_DEBUG=1`.

## Crear un usuario médico

```bash
flask --app app crear-medico   # pide nombre de usuario y contraseña
```

## Endpoints

### Para la Pico (encabezado `X-Redux-Key`)

| Método | Ruta | Descripción |
|--------|------|-------------|
| GET | `/api/paciente/<id>` | Devuelve `{"id": 3, "nombre": "..."}`; 404 si no existe |
| POST | `/api/sesion` | Guarda una sesión con sus repeticiones |

Cuerpo de `POST /api/sesion`:

```json
{
  "id_envio": "a1b2c3d4",
  "id_paciente": 3,
  "brazo": "derecho",
  "rom_calibrado_grados": 92.5,
  "duracion_s": 310,
  "repeticiones": [
    {"numero": 1, "rango_grados": 85.2, "asistencia_pct": 40.0, "duracion_ms": 2300}
  ]
}
```

Respuestas: `201 {"id_sesion": 17}`; `200` con el id ya guardado si el `id_envio`
se repite (no duplica); `400` indicando qué campo falló; `404` si el paciente no
existe. Validaciones: paciente existente, `brazo` izquierdo/derecho, rangos 0–180,
asistencia 0–100, máximo 500 repeticiones. Los promedios (`rango_medio_grados`,
`asistencia_media_pct`) se calculan al guardar.

### Para la web (inicio de sesión + roles)

- `GET/POST /ingreso`, `GET /salir`
- Páginas: `/pacientes`, `/paciente/<id>/avance`, `/sesion/<id>`
- JSON (consumido por `fetch`): `/api/web/pacientes`, `/api/web/paciente/<id>`
  (filtro opcional `?brazo=`), `/api/web/sesion/<id>`, y alta/edición de pacientes
  y creación de cuentas (solo médico).

El **médico** ve y administra todo; el **paciente** solo accede a sus propios datos.

## Herramientas (`tools/`)

```bash
# Simular una sesión como lo haría la Pico (el servidor debe estar corriendo)
python tools/simular_sesion.py --paciente 3 --brazo derecho --repeticiones 10

# Migrar SOLO el paciente REDUX desde la base vieja (sin lecturas viejas)
python tools/migrar_paciente.py
```

## Pruebas

```bash
python -m pytest
```

Cubren los endpoints de la Pico, los permisos por rol (un paciente no puede ver a
otro) y el manejo de UTF-8 (nombres con ñ y tildes).

## Estructura

```
API/
  app.py          # create_app() y punto de entrada
  config.py       # claves desde entorno/.env
  extensions.py   # db y login_manager
  models.py       # paciente, usuario, sesion, repeticion
  api_pico.py     # endpoints X-Redux-Key
  api_web.py      # endpoints JSON de la web
  paginas.py      # rutas que sirven las páginas
  seguridad.py    # decoradores de acceso JSON
  cli.py          # comando crear-medico
  tools/          # simulador y migración
  tests/          # pytest
```
