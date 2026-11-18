"""Pruebas de inicio de sesion, roles y acceso (un paciente no ve a otro)."""
from conftest import ingresar


def test_web_sin_login_devuelve_401(client, datos):
    assert client.get("/api/web/pacientes").status_code == 401
    assert client.get("/api/web/paciente/3").status_code == 401


def test_medico_ve_lista_de_pacientes(client, datos):
    ingresar(client, "doc", "1234")
    r = client.get("/api/web/pacientes")
    assert r.status_code == 200
    ids = {p["id"] for p in r.get_json()}
    assert ids == {3, 5}


def test_login_incorrecto(client, datos):
    r = ingresar(client, "doc", "clave-mala")
    assert r.status_code == 401


def test_paciente_ve_lo_suyo(client, datos):
    ingresar(client, "pac3", "abcd")
    assert client.get("/api/web/paciente/3").status_code == 200
    assert client.get("/paciente/3/avance").status_code == 200


def test_paciente_no_ve_a_otro_paciente(client, datos):
    ingresar(client, "pac3", "abcd")
    # Endpoint JSON de otro paciente.
    assert client.get("/api/web/paciente/5").status_code == 403
    # Pagina de avance de otro paciente.
    assert client.get("/paciente/5/avance").status_code == 403


def test_paciente_no_accede_a_la_lista(client, datos):
    ingresar(client, "pac3", "abcd")
    assert client.get("/api/web/pacientes").status_code == 403


def test_paciente_no_ve_sesion_de_otro(app, client, datos):
    # Creamos una sesion para el paciente 5 y verificamos que pac3 no la vea.
    from conftest import cabecera_pico, sesion_valida

    client.post(
        "/api/sesion",
        json=sesion_valida(id_envio="p5", id_paciente=5),
        headers=cabecera_pico(),
    )
    ingresar(client, "pac3", "abcd")
    assert client.get("/api/web/sesion/1").status_code == 403


def test_solo_medico_crea_cuentas(client, datos):
    ingresar(client, "pac3", "abcd")
    r = client.post(
        "/api/web/paciente/3/cuenta",
        json={"nombre_usuario": "x", "contrasena": "abcd"},
    )
    assert r.status_code == 403


def test_crear_cuenta_de_paciente_y_luego_ingresar(client, datos):
    # El medico crea la cuenta del paciente 5 (que todavia no tenia).
    ingresar(client, "doc", "1234")
    r = client.post(
        "/api/web/paciente/5/cuenta",
        json={"nombre_usuario": "pac5", "contrasena": "clave5"},
    )
    assert r.status_code == 201
    assert r.get_json()["nombre_usuario"] == "pac5"
    client.get("/salir")

    # Se inicia sesion con la cuenta recien creada (302 = login ok y redirige).
    assert ingresar(client, "pac5", "clave5").status_code == 302

    # Y solo accede a su propio paciente (5), no a otro.
    assert client.get("/api/web/paciente/5").status_code == 200
    assert client.get("/api/web/paciente/3").status_code == 403
