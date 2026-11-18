"""Fixtures compartidas de las pruebas."""
import sys
from pathlib import Path

import pytest

# Permite importar app/models (estan en API/, un nivel arriba de tests/).
API_DIR = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(API_DIR))

from app import create_app  # noqa: E402
from config import Config  # noqa: E402
from extensions import db as _db  # noqa: E402
from models import Paciente, Usuario  # noqa: E402

CLAVE_TEST = "clave-test"


@pytest.fixture
def app(tmp_path):
    archivo = tmp_path / "test.db"

    class ConfigTest(Config):
        TESTING = True
        SECRET_KEY = "test-secret"
        REDUX_API_KEY = CLAVE_TEST
        SQLALCHEMY_DATABASE_URI = f"sqlite:///{archivo.as_posix()}"
        DEBUG = False

    return create_app(ConfigTest)


@pytest.fixture
def client(app):
    return app.test_client()


@pytest.fixture
def datos(app):
    """Siembra dos pacientes y dos usuarios (medico y paciente de #3)."""
    with app.app_context():
        p3 = Paciente(id=3, nombre="Muñoz", edad=40, observaciones="Prueba")
        p5 = Paciente(id=5, nombre="Otro Paciente", edad=30)
        medico = Usuario(nombre_usuario="doc", rol="medico")
        medico.set_contrasena("1234")
        paciente = Usuario(nombre_usuario="pac3", rol="paciente", paciente_id=3)
        paciente.set_contrasena("abcd")
        _db.session.add_all([p3, p5, medico, paciente])
        _db.session.commit()


def ingresar(client, usuario, contrasena):
    return client.post(
        "/ingreso",
        data={"nombre_usuario": usuario, "contrasena": contrasena},
    )


def cabecera_pico():
    return {"X-Redux-Key": CLAVE_TEST}


def sesion_valida(**cambios):
    cuerpo = {
        "id_envio": "a1b2c3d4",
        "id_paciente": 3,
        "brazo": "derecho",
        "rom_calibrado_grados": 92.5,
        "duracion_s": 310,
        "repeticiones": [
            {"numero": 1, "rango_grados": 85.2, "asistencia_pct": 40.0, "duracion_ms": 2300},
            {"numero": 2, "rango_grados": 90.0, "asistencia_pct": 60.0, "duracion_ms": 2100},
        ],
    }
    cuerpo.update(cambios)
    return cuerpo
