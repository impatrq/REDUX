"""Simulador: manda una sesion de prueba a la API tal como lo haria la Pico.

Uso:
    python tools/simular_sesion.py --paciente 3 --brazo derecho --repeticiones 10

La clave se toma del argumento --key o de la variable de entorno REDUX_API_KEY
(o del .env de API/). El servidor debe estar corriendo (flask run).
"""
import argparse
import os
import random
import sys
import uuid
from pathlib import Path

import requests
from dotenv import load_dotenv

# Carga API/.env (este archivo esta en API/tools/).
load_dotenv(Path(__file__).resolve().parent.parent / ".env")


def construir_sesion(id_paciente, brazo, cantidad, rom=92.5, duracion_s=310):
    """Arma el cuerpo JSON exacto que espera POST /api/sesion."""
    repeticiones = []
    for numero in range(1, cantidad + 1):
        # El rango ronda el ROM calibrado; la asistencia baja con las reps.
        rango = round(min(rom, max(0.0, random.gauss(rom * 0.9, 6))), 1)
        asistencia = round(max(0.0, 60 - numero * (40 / max(cantidad, 1)) + random.uniform(-5, 5)), 1)
        asistencia = min(100.0, asistencia)
        repeticiones.append(
            {
                "numero": numero,
                "rango_grados": rango,
                "asistencia_pct": asistencia,
                "duracion_ms": random.randint(1800, 2600),
            }
        )
    return {
        "id_envio": uuid.uuid4().hex[:8],
        "id_paciente": id_paciente,
        "brazo": brazo,
        "rom_calibrado_grados": rom,
        "duracion_s": duracion_s,
        "repeticiones": repeticiones,
    }


def main():
    parser = argparse.ArgumentParser(description="Simula una sesion de REDUX.")
    parser.add_argument("--url", default="http://localhost:5000")
    parser.add_argument("--paciente", type=int, default=1)
    parser.add_argument("--brazo", choices=["izquierdo", "derecho"], default="derecho")
    parser.add_argument("--repeticiones", type=int, default=10)
    parser.add_argument("--rom", type=float, default=92.5)
    parser.add_argument("--duracion", type=int, default=310)
    parser.add_argument("--key", default=os.environ.get("REDUX_API_KEY"))
    args = parser.parse_args()

    if not args.key:
        print("Falta la clave: pasa --key o define REDUX_API_KEY en el entorno/.env.")
        sys.exit(1)

    cuerpo = construir_sesion(
        args.paciente, args.brazo, args.repeticiones, args.rom, args.duracion
    )
    print(f"Enviando sesion id_envio={cuerpo['id_envio']} "
          f"({args.repeticiones} repeticiones, brazo {args.brazo})...")
    resp = requests.post(
        args.url.rstrip("/") + "/api/sesion",
        json=cuerpo,
        headers={"X-Redux-Key": args.key},
        timeout=10,
    )
    print("Respuesta:", resp.status_code, resp.json())


if __name__ == "__main__":
    main()
