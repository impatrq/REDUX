# Web_datos — páginas de datos de REDUX

Páginas del panel de datos de REDUX. Las **sirve la app Flask de
[`../API`](../API)** (su `template_folder` y `static_folder` apuntan a esta
carpeta); no es un proyecto aparte. Las páginas piden los datos por `fetch` a los
endpoints `/api/web/...` de la API.

Estilo: violeta de REDUX, **Bootstrap 5.3** y **Chart.js** (desde CDN). Todo en
UTF-8 (`<meta charset="utf-8">`) para que los nombres con ñ y tildes se vean bien.

## Páginas

| Archivo | Ruta | Para quién | Qué muestra |
|---------|------|-----------|-------------|
| `ingreso.html` | `/ingreso` | Todos | Inicio de sesión |
| `pacientes.html` | `/pacientes` | Médico | Lista (ID, nombre, edad, última sesión) con botones Nuevo, Editar, Ver avance y Crear cuenta |
| `avance.html` | `/paciente/<id>/avance` | Médico o el propio paciente | Encabezado ID+nombre, filtro por brazo, gráfico de evolución (rango medio y asistencia media por sesión) y tabla de sesiones |
| `sesion.html` | `/sesion/<id>` | Médico o el propio paciente | Fecha, brazo, rango calibrado, duración, y gráfico + tabla de repeticiones |

`base.html` es el layout común (barra, estilos y scripts). `static/redux.css` tiene
la paleta violeta y `static/app.js` los helpers de `fetch` y formato.

## Cómo verlas

Se levantan junto con la API:

```bash
cd ../API
flask --app app run
# luego abrir http://localhost:5000/ingreso
```

Un **paciente** solo puede ver su propio avance y sus sesiones; el acceso a datos
de otro paciente devuelve 403.
