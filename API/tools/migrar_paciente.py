"""Migra SOLO el paciente REDUX desde la base vieja a la base nueva.

Copia unicamente nombre, edad y observaciones. NO copia ninguna lectura vieja.
La base vieja se abre en modo SOLO LECTURA y nunca se modifica.

Uso:
    python tools/migrar_paciente.py
    python tools/migrar_paciente.py --origen "C:/ruta/redux.db" --nombre REDUX
"""
import argparse
import sqlite3
import sys
from pathlib import Path

# Permite importar app/models estando en API/tools/.
API_DIR = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(API_DIR))

from app import create_app  # noqa: E402
from extensions import db  # noqa: E402
from models import Paciente  # noqa: E402

ORIGEN_POR_DEFECTO = r"C:\Users\benic\Redux_Backend\instance\redux.db"


def leer_paciente_viejo(origen: Path, nombre: str):
    """Lee el paciente por nombre de la base vieja (solo lectura)."""
    if not origen.exists():
        raise FileNotFoundError(f"No se encontro la base vieja en: {origen}")
    # file:...?mode=ro abre la base sin posibilidad de escribir.
    uri = f"file:{origen.as_posix()}?mode=ro"
    conn = sqlite3.connect(uri, uri=True)
    try:
        conn.row_factory = sqlite3.Row
        fila = conn.execute(
            "SELECT id, nombre, edad, observaciones FROM paciente WHERE nombre = ?",
            (nombre,),
        ).fetchone()
    finally:
        conn.close()
    return fila


def main():
    parser = argparse.ArgumentParser(description="Migra el paciente REDUX.")
    parser.add_argument("--origen", default=ORIGEN_POR_DEFECTO)
    parser.add_argument("--nombre", default="REDUX")
    args = parser.parse_args()

    fila = leer_paciente_viejo(Path(args.origen), args.nombre)
    if fila is None:
        print(f"No se encontro un paciente llamado '{args.nombre}' en la base vieja.")
        sys.exit(1)

    app = create_app()
    with app.app_context():
        existente = db.session.get(Paciente, fila["id"])
        if existente is not None:
            print(f"Ya existe un paciente con id {fila['id']} ('{existente.nombre}'). No se hace nada.")
            return
        paciente = Paciente(
            id=fila["id"],
            nombre=fila["nombre"],
            edad=fila["edad"],
            observaciones=fila["observaciones"],
        )
        db.session.add(paciente)
        db.session.commit()
        print(f"Paciente '{paciente.nombre}' migrado con id {paciente.id} (sin lecturas viejas).")


if __name__ == "__main__":
    main()
