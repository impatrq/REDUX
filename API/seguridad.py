"""Decoradores de acceso para los endpoints JSON de la web.

Las paginas usan @login_required de Flask-Login (que redirige al ingreso).
Los endpoints /api/web/... usan estos decoradores, que devuelven JSON en vez
de redirigir.
"""
from functools import wraps

from flask import jsonify
from flask_login import current_user


def sesion_json(f):
    """Requiere usuario autenticado; si no, 401 JSON."""

    @wraps(f)
    def wrapper(*args, **kwargs):
        if not current_user.is_authenticated:
            return jsonify({"error": "Necesitas iniciar sesion"}), 401
        return f(*args, **kwargs)

    return wrapper


def medico_json(f):
    """Requiere usuario autenticado con rol medico; si no, 401/403 JSON."""

    @wraps(f)
    def wrapper(*args, **kwargs):
        if not current_user.is_authenticated:
            return jsonify({"error": "Necesitas iniciar sesion"}), 401
        if not current_user.es_medico:
            return jsonify({"error": "Solo el medico puede hacer esto"}), 403
        return f(*args, **kwargs)

    return wrapper
