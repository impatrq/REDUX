"""Comandos de linea de la API de REDUX."""
import click

from extensions import db
from models import Usuario


def registrar_cli(app):
    @app.cli.command("crear-medico")
    def crear_medico():
        """Crea un usuario con rol medico (pide usuario y contrasena)."""
        nombre = click.prompt("Nombre de usuario").strip()
        if not nombre:
            click.echo("El nombre de usuario no puede quedar vacio.")
            return
        if Usuario.query.filter_by(nombre_usuario=nombre).first() is not None:
            click.echo(f"Ya existe un usuario llamado '{nombre}'.")
            return
        contrasena = click.prompt(
            "Contrasena", hide_input=True, confirmation_prompt=True
        )
        if len(contrasena) < 4:
            click.echo("La contrasena debe tener al menos 4 caracteres.")
            return

        usuario = Usuario(nombre_usuario=nombre[:50], rol="medico")
        usuario.set_contrasena(contrasena)
        db.session.add(usuario)
        db.session.commit()
        click.echo(f"Medico '{nombre}' creado (id {usuario.id}).")
