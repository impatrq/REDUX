"""Rutas que sirven las paginas HTML de Web_datos/ e inicio/cierre de sesion.

Las paginas piden los datos por fetch a /api/web/...; aca solo se controla el
acceso y se entregan las plantillas.
"""
from flask import (
    Blueprint,
    abort,
    redirect,
    render_template,
    request,
    url_for,
)
from flask_login import current_user, login_required, login_user, logout_user

from extensions import db
from models import Paciente, Sesion, Usuario

bp = Blueprint("paginas", __name__)


@bp.get("/")
def inicio():
    if not current_user.is_authenticated:
        return redirect(url_for("paginas.ingreso"))
    if current_user.es_medico:
        return redirect(url_for("paginas.pacientes"))
    return redirect(url_for("paginas.avance", id=current_user.paciente_id))


@bp.get("/ingreso")
def ingreso():
    if current_user.is_authenticated:
        return redirect(url_for("paginas.inicio"))
    return render_template("ingreso.html")


@bp.post("/ingreso")
def procesar_ingreso():
    nombre_usuario = (request.form.get("nombre_usuario") or "").strip()
    contrasena = request.form.get("contrasena") or ""
    usuario = Usuario.query.filter_by(nombre_usuario=nombre_usuario).first()
    if usuario is None or not usuario.verificar_contrasena(contrasena):
        return render_template(
            "ingreso.html", error="Usuario o contrasena incorrectos"
        ), 401
    login_user(usuario)
    return redirect(url_for("paginas.inicio"))


@bp.get("/salir")
@login_required
def salir():
    logout_user()
    return redirect(url_for("paginas.ingreso"))


@bp.get("/pacientes")
@login_required
def pacientes():
    if not current_user.es_medico:
        # Un paciente no tiene lista: va a su propio avance.
        return redirect(url_for("paginas.avance", id=current_user.paciente_id))
    return render_template("pacientes.html")


@bp.get("/paciente/<int:id>/avance")
@login_required
def avance(id):
    if not current_user.puede_ver_paciente(id):
        abort(403)
    paciente = db.session.get(Paciente, id)
    if paciente is None:
        abort(404)
    return render_template("avance.html", paciente=paciente)


@bp.get("/sesion/<int:id>")
@login_required
def sesion(id):
    s = db.session.get(Sesion, id)
    if s is None:
        abort(404)
    if not current_user.puede_ver_paciente(s.paciente_id):
        abort(403)
    return render_template("sesion.html", sesion_id=id, paciente=s.paciente)
