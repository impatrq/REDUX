"""Prueba de UTF-8 de punta a punta: guardar y leer un nombre con enie."""
from conftest import cabecera_pico, ingresar


def test_guardar_y_leer_munoz_en_la_base(app):
    from extensions import db
    from models import Paciente

    with app.app_context():
        db.session.add(Paciente(id=10, nombre="Muñoz", observaciones="ññ áéí"))
        db.session.commit()

    with app.app_context():
        p = db.session.get(Paciente, 10)
        assert p.nombre == "Muñoz"
        assert p.observaciones == "ññ áéí"


def test_api_pico_devuelve_munoz_sin_escapar(client, datos):
    # El paciente 3 se llama "Muñoz" (ver fixture datos).
    r = client.get("/api/paciente/3", headers=cabecera_pico())
    assert r.get_json()["nombre"] == "Muñoz"
    # ensure_ascii=False: la enie viaja tal cual, no como ñ.
    assert "Muñoz" in r.get_data(as_text=True)
    assert "\\u00f1" not in r.get_data(as_text=True)


def test_crear_paciente_con_enie_por_la_web(client, datos):
    ingresar(client, "doc", "1234")
    r = client.post(
        "/api/web/pacientes",
        json={"id": 20, "nombre": "Muñoz Núñez", "edad": 50},
    )
    assert r.status_code == 201
    lista = client.get("/api/web/pacientes").get_json()
    nombres = {p["nombre"] for p in lista}
    assert "Muñoz Núñez" in nombres
