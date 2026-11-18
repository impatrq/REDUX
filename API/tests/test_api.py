"""Pruebas de los endpoints de la Pico (X-Redux-Key)."""
from conftest import cabecera_pico, sesion_valida


def test_get_paciente_requiere_clave(client, datos):
    assert client.get("/api/paciente/3").status_code == 401
    assert client.get("/api/paciente/3", headers={"X-Redux-Key": "mala"}).status_code == 401


def test_get_paciente_devuelve_solo_id_y_nombre(client, datos):
    r = client.get("/api/paciente/3", headers=cabecera_pico())
    assert r.status_code == 200
    assert r.get_json() == {"id": 3, "nombre": "Muñoz"}


def test_get_paciente_inexistente(client, datos):
    assert client.get("/api/paciente/999", headers=cabecera_pico()).status_code == 404


def test_post_sesion_crea(client, datos):
    r = client.post("/api/sesion", json=sesion_valida(), headers=cabecera_pico())
    assert r.status_code == 201
    assert "id_sesion" in r.get_json()


def test_post_sesion_promedios_correctos(app, client, datos):
    from extensions import db
    from models import Sesion

    client.post("/api/sesion", json=sesion_valida(), headers=cabecera_pico())
    with app.app_context():
        s = db.session.get(Sesion, 1)
        assert s.repeticiones == 2
        assert s.rango_medio_grados == 87.6  # (85.2 + 90.0) / 2
        assert s.asistencia_media_pct == 50.0  # (40 + 60) / 2
        assert s.brazo == "derecho"


def test_post_sesion_dedupe_por_id_envio(client, datos):
    r1 = client.post("/api/sesion", json=sesion_valida(), headers=cabecera_pico())
    assert r1.status_code == 201
    primero = r1.get_json()["id_sesion"]
    # Mismo id_envio: no duplica y responde 200 con el id ya guardado.
    r2 = client.post("/api/sesion", json=sesion_valida(), headers=cabecera_pico())
    assert r2.status_code == 200
    assert r2.get_json()["id_sesion"] == primero


def test_post_sesion_paciente_inexistente(client, datos):
    r = client.post(
        "/api/sesion", json=sesion_valida(id_paciente=777), headers=cabecera_pico()
    )
    assert r.status_code == 404


def test_brazo_invalido(client, datos):
    r = client.post("/api/sesion", json=sesion_valida(brazo="centro"), headers=cabecera_pico())
    assert r.status_code == 400
    assert "brazo" in r.get_json()["error"]


def test_brazo_valido_izquierdo(client, datos):
    r = client.post("/api/sesion", json=sesion_valida(brazo="izquierdo"), headers=cabecera_pico())
    assert r.status_code == 201


def test_rom_fuera_de_rango(client, datos):
    assert client.post(
        "/api/sesion", json=sesion_valida(rom_calibrado_grados=181), headers=cabecera_pico()
    ).status_code == 400
    assert client.post(
        "/api/sesion", json=sesion_valida(rom_calibrado_grados=-1), headers=cabecera_pico()
    ).status_code == 400


def test_rango_grados_fuera_de_rango(client, datos):
    malo = sesion_valida(repeticiones=[{"numero": 1, "rango_grados": 200, "asistencia_pct": 50}])
    assert client.post("/api/sesion", json=malo, headers=cabecera_pico()).status_code == 400


def test_asistencia_fuera_de_rango(client, datos):
    malo = sesion_valida(repeticiones=[{"numero": 1, "rango_grados": 50, "asistencia_pct": 101}])
    assert client.post("/api/sesion", json=malo, headers=cabecera_pico()).status_code == 400


def test_maximo_500_repeticiones(client, datos):
    muchas = [{"numero": i, "rango_grados": 50, "asistencia_pct": 50} for i in range(501)]
    r = client.post("/api/sesion", json=sesion_valida(repeticiones=muchas), headers=cabecera_pico())
    assert r.status_code == 400
    # 500 exactas si estan bien.
    justas = [{"numero": i, "rango_grados": 50, "asistencia_pct": 50} for i in range(500)]
    r2 = client.post(
        "/api/sesion", json=sesion_valida(id_envio="otra", repeticiones=justas), headers=cabecera_pico()
    )
    assert r2.status_code == 201


def test_lista_repeticiones_vacia(client, datos):
    r = client.post("/api/sesion", json=sesion_valida(repeticiones=[]), headers=cabecera_pico())
    assert r.status_code == 400


def test_falta_id_envio(client, datos):
    cuerpo = sesion_valida()
    del cuerpo["id_envio"]
    assert client.post("/api/sesion", json=cuerpo, headers=cabecera_pico()).status_code == 400
