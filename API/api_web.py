"""Endpoints JSON que consumen las paginas de Web_datos/ por fetch.

Protegidos con inicio de sesion. El medico ve y administra todo; el paciente
solo accede a sus propios datos.
"""
from flask import Blueprint, jsonify, request
from flask_login import current_user

from extensions import db
from models import Paciente, Sesion, Usuario, a_utc_iso
from seguridad import medico_json, sesion_json

bp = Blueprint("api_web", __name__, url_prefix="/api/web")

BRAZOS_VALIDOS = {"izquierdo", "derecho"}


def _ultima_sesion(paciente):
    sesion = (
        Sesion.query.filter_by(paciente_id=paciente.id)
        .order_by(Sesion.fecha.desc())
        .first()
    )
    return a_utc_iso(sesion.fecha) if sesion else None


@bp.get("/pacientes")
@medico_json
def listar_pacientes():
    pacientes = Paciente.query.order_by(Paciente.id).all()
    return jsonify(
        [
            {
                "id": p.id,
                "nombre": p.nombre,
                "edad": p.edad,
                "observaciones": p.observaciones,
                "ultima_sesion": _ultima_sesion(p),
                "tiene_cuenta": bool(p.usuarios),
            }
            for p in pacientes
        ]
    )


@bp.post("/pacientes")
@medico_json
def crear_paciente():
    datos = request.get_json(silent=True) or {}
    # El id es el numero del panel del exoesqueleto: lo asigna el medico.
    pid = datos.get("id")
    if not isinstance(pid, int) or isinstance(pid, bool) or pid <= 0:
        return jsonify({"error": "El id (numero del panel) debe ser un entero positivo"}), 400
    if db.session.get(Paciente, pid) is not None:
        return jsonify({"error": f"Ya existe un paciente con id {pid}"}), 409
    nombre = (datos.get("nombre") or "").strip()
    if not nombre:
        return jsonify({"error": "El nombre es obligatorio"}), 400

    paciente = Paciente(
        id=pid,
        nombre=nombre[:100],
        edad=datos.get("edad") if isinstance(datos.get("edad"), int) else None,
        observaciones=(datos.get("observaciones") or None),
    )
    db.session.add(paciente)
    db.session.commit()
    return jsonify({"id": paciente.id}), 201


@bp.put("/paciente/<int:id>")
@medico_json
def editar_paciente(id):
    paciente = db.session.get(Paciente, id)
    if paciente is None:
        return jsonify({"error": "Paciente no encontrado"}), 404
    datos = request.get_json(silent=True) or {}
    if "nombre" in datos:
        nombre = (datos.get("nombre") or "").strip()
        if not nombre:
            return jsonify({"error": "El nombre no puede quedar vacio"}), 400
        paciente.nombre = nombre[:100]
    if "edad" in datos:
        paciente.edad = datos["edad"] if isinstance(datos["edad"], int) else None
    if "observaciones" in datos:
        paciente.observaciones = datos["observaciones"] or None
    db.session.commit()
    return jsonify({"mensaje": "Paciente actualizado"}), 200


@bp.post("/paciente/<int:id>/cuenta")
@medico_json
def crear_cuenta_paciente(id):
    paciente = db.session.get(Paciente, id)
    if paciente is None:
        return jsonify({"error": "Paciente no encontrado"}), 404
    datos = request.get_json(silent=True) or {}
    nombre_usuario = (datos.get("nombre_usuario") or "").strip()
    contrasena = datos.get("contrasena") or ""
    if not nombre_usuario:
        return jsonify({"error": "El nombre de usuario es obligatorio"}), 400
    if len(contrasena) < 4:
        return jsonify({"error": "La contrasena debe tener al menos 4 caracteres"}), 400
    if Usuario.query.filter_by(nombre_usuario=nombre_usuario).first() is not None:
        return jsonify({"error": "Ese nombre de usuario ya existe"}), 409

    usuario = Usuario(
        nombre_usuario=nombre_usuario[:50],
        rol="paciente",
        paciente_id=paciente.id,
    )
    usuario.set_contrasena(contrasena)
    db.session.add(usuario)
    db.session.commit()
    return jsonify({"id": usuario.id, "nombre_usuario": usuario.nombre_usuario}), 201


@bp.get("/paciente/<int:id>")
@sesion_json
def ver_paciente(id):
    if not current_user.puede_ver_paciente(id):
        return jsonify({"error": "No podes ver los datos de otro paciente"}), 403
    paciente = db.session.get(Paciente, id)
    if paciente is None:
        return jsonify({"error": "Paciente no encontrado"}), 404

    consulta = Sesion.query.filter_by(paciente_id=id)
    brazo = request.args.get("brazo")
    if brazo in BRAZOS_VALIDOS:
        consulta = consulta.filter_by(brazo=brazo)
    sesiones = consulta.order_by(Sesion.fecha).all()

    return jsonify(
        {
            "id": paciente.id,
            "nombre": paciente.nombre,
            "edad": paciente.edad,
            "observaciones": paciente.observaciones,
            "sesiones": [s.resumen() for s in sesiones],
        }
    )


@bp.get("/sesion/<int:id>")
@sesion_json
def ver_sesion(id):
    sesion = db.session.get(Sesion, id)
    if sesion is None:
        return jsonify({"error": "Sesion no encontrada"}), 404
    if not current_user.puede_ver_paciente(sesion.paciente_id):
        return jsonify({"error": "No podes ver sesiones de otro paciente"}), 403

    datos = sesion.resumen()
    datos["paciente_nombre"] = sesion.paciente.nombre
    datos["detalle"] = [r.resumen() for r in sesion.detalle]
    return jsonify(datos)
