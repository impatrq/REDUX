"""API de REDUX: una sola app Flask que expone la API y sirve las paginas
de Web_datos/. Punto de entrada y fabrica de la aplicacion.

    flask --app app run        (desarrollo)
    python app.py              (equivalente)
    flask --app app crear-medico
"""
from pathlib import Path

from flask import Flask

from config import Config
from extensions import db, login_manager

BASEDIR = Path(__file__).resolve().parent
WEB_DATOS = BASEDIR.parent / "Web_datos"


def create_app(config_object=Config):
    app = Flask(
        __name__,
        instance_relative_config=True,
        template_folder=str(WEB_DATOS / "templates"),
        static_folder=str(WEB_DATOS / "static"),
    )
    app.config.from_object(config_object)

    # JSON con tildes y enies tal cual (no \uXXXX).
    app.json.ensure_ascii = False

    db.init_app(app)
    login_manager.init_app(app)
    login_manager.login_view = "paginas.ingreso"

    # Blueprints
    from api_pico import bp as bp_pico

    app.register_blueprint(bp_pico)

    with app.app_context():
        db.create_all()

    return app


if __name__ == "__main__":
    app = create_app()
    # Debug apagado por defecto; host 0.0.0.0 para la red local.
    app.run(host="0.0.0.0", port=5000, debug=app.config["DEBUG"])
