"""Extensiones de Flask compartidas.

Se instancian acá (sin app) para evitar imports circulares y para poder
inicializarlas con distintas apps (por ejemplo, la app de las pruebas).
"""
from flask_sqlalchemy import SQLAlchemy
from flask_login import LoginManager

db = SQLAlchemy()
login_manager = LoginManager()
