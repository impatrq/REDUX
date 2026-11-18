"""Configuracion de la API de REDUX.

La clave de la Pico (REDUX_API_KEY) y la SECRET_KEY de Flask se leen de
variables de entorno o de un archivo .env (que NO se versiona). El modo debug
esta apagado por defecto.
"""
import os
from pathlib import Path

from dotenv import load_dotenv

BASEDIR = Path(__file__).resolve().parent

# Carga API/.env si existe (no falla si no esta).
load_dotenv(BASEDIR / ".env")


def _flag(nombre: str, por_defecto: bool = False) -> bool:
    valor = os.environ.get(nombre)
    if valor is None:
        return por_defecto
    return valor.strip().lower() in {"1", "true", "yes", "on", "si"}


class Config:
    # Clave del encabezado X-Redux-Key que usa la Pico.
    REDUX_API_KEY = os.environ.get("REDUX_API_KEY", "clave-de-desarrollo-redux")
    # Clave de sesiones de Flask-Login.
    SECRET_KEY = os.environ.get("SECRET_KEY", "secret-de-desarrollo-redux")

    # Base SQLite en API/instance/redux.db (instance_path lo arma Flask).
    SQLALCHEMY_DATABASE_URI = os.environ.get(
        "DATABASE_URL", "sqlite:///redux.db"
    )
    SQLALCHEMY_TRACK_MODIFICATIONS = False

    # Debug apagado salvo que se pida explicitamente por entorno.
    DEBUG = _flag("FLASK_DEBUG", False)

    # Limite de repeticiones aceptadas en una sesion.
    MAX_REPETICIONES = 500
