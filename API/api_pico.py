"""Endpoints para la Pico (dispositivo), protegidos con el encabezado
X-Redux-Key. Respuestas minimas porque la Pico tiene poca memoria.
"""
from functools import wraps

from flask import Blueprint, current_app, jsonify, request

from extensions import db
from models import Paciente, Repeticion, Sesion

bp = Blueprint("api_pico", __name__, url_prefix="/api")

BRAZOS_VALIDOS = {"izquierdo", "derecho"}


def requiere_clave(f):
    """Valida el encabezado X-Redux-Key contra REDUX_API_KEY."""

    @wraps(f)
    def wrapper(*args, **kwargs):
        clave = request.headers.get("X-Redux-Key")
        if not clave or clave != current_app.config["REDUX_API_KEY"]:
            return jsonify({"error": "Clave invalida"}), 401
        return f(*args, **kwargs)

    return wrapper


def _numero(valor):
    """Devuelve el valor como float si es numerico (no booleano), o None."""
    if isinstance(valor, bool):
        return None
    if isinstance(valor, (int, float)):
        return float(valor)
    return None


@bp.get("/paciente/<int:id>")
@requiere_clave
def obtener_paciente(id):
    paciente = db.session.get(Paciente, id)
    if paciente is None:
        return jsonify({"error": "Paciente no encontrado"}), 404
    # Solo id y nombre: nada mas.
    return jsonify({"id": paciente.id, "nombre": paciente.nombre}), 200


@bp.post("/sesion")
@requiere_clave
def crear_sesion():
    datos = request.get_json(silent=True)
    if not isinstance(datos, dict):
        return jsonify({"error": "Se esperaba un JSON con los datos de la sesion"}), 400

    # --- id_envio ---
    id_envio = datos.get("id_envio")
    if not isinstance(id_envio, str) or not id_envio.strip():
        return jsonify({"error": "id_envio es obligatorio (texto no vacio)"}), 400
    id_envio = id_envio.strip()

    # Dedupe: si ya existe esa sesion, no se duplica.
    existente = Sesion.query.filter_by(id_envio=id_envio).first()
    if existente is not None:
        return jsonify({"id_sesion": existente.id}), 200

    # --- id_paciente ---
    id_paciente = datos.get("id_paciente")
    if not isinstance(id_paciente, int) or isinstance(id_paciente, bool):
        return jsonify({"error": "id_paciente es obligatorio (numero entero)"}), 400

    paciente = db.session.get(Paciente, id_paciente)
    if paciente is None:
        return jsonify({"error": "Paciente no encontrado"}), 404

    # --- brazo ---
    brazo = datos.get("brazo")
    if brazo not in BRAZOS_VALIDOS:
        return jsonify({"error": "brazo debe ser 'izquierdo' o 'derecho'"}), 400

    # --- rom_calibrado_grados ---
    rom = _numero(datos.get("rom_calibrado_grados"))
    if rom is None or not (0 <= rom <= 180):
        return jsonify({"error": "rom_calibrado_grados debe estar entre 0 y 180"}), 400

    # --- duracion_s ---
    duracion = _numero(datos.get("duracion_s"))
    if duracion is None or duracion < 0:
        return jsonify({"error": "duracion_s debe ser un numero mayor o igual a 0"}), 400

    # --- repeticiones ---
    lista = datos.get("repeticiones")
    if not isinstance(lista, list) or len(lista) == 0:
        return jsonify({"error": "repeticiones debe ser una lista con al menos un elemento"}), 400
    maximo = current_app.config["MAX_REPETICIONES"]
    if len(lista) > maximo:
        return jsonify({"error": f"demasiadas repeticiones (maximo {maximo})"}), 400

    repeticiones = []
    suma_rango = 0.0
    suma_asistencia = 0.0
    for i, rep in enumerate(lista, start=1):
        if not isinstance(rep, dict):
            return jsonify({"error": f"repeticion #{i} invalida"}), 400

        numero = rep.get("numero", i)
        if not isinstance(numero, int) or isinstance(numero, bool):
            return jsonify({"error": f"repeticion #{i}: numero debe ser entero"}), 400

        rango = _numero(rep.get("rango_grados"))
        if rango is None or not (0 <= rango <= 180):
            return jsonify({"error": f"repeticion #{i}: rango_grados debe estar entre 0 y 180"}), 400

        asistencia = _numero(rep.get("asistencia_pct"))
        if asistencia is None or not (0 <= asistencia <= 100):
            return jsonify({"error": f"repeticion #{i}: asistencia_pct debe estar entre 0 y 100"}), 400

        duracion_ms = rep.get("duracion_ms")
        if duracion_ms is not None:
            d = _numero(duracion_ms)
            if d is None or d < 0:
                return jsonify({"error": f"repeticion #{i}: duracion_ms invalida"}), 400
            duracion_ms = int(d)

        suma_rango += rango
        suma_asistencia += asistencia
        repeticiones.append(
            Repeticion(
                numero=numero,
                rango_grados=rango,
                asistencia_pct=asistencia,
                duracion_ms=duracion_ms,
            )
        )

    cantidad = len(repeticiones)
    # Promedios calculados al guardar.
    sesion = Sesion(
        paciente_id=paciente.id,
        id_envio=id_envio,
        brazo=brazo,
        rom_calibrado_grados=rom,
        duracion_s=int(duracion),
        repeticiones=cantidad,
        rango_medio_grados=round(suma_rango / cantidad, 2),
        asistencia_media_pct=round(suma_asistencia / cantidad, 2),
        detalle=repeticiones,
    )
    db.session.add(sesion)
    db.session.commit()

    return jsonify({"id_sesion": sesion.id}), 201
