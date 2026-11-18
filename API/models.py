"""Modelos de datos de REDUX (SQLAlchemy).

Tablas: paciente, usuario, sesion, repeticion.
"""
from datetime import datetime, timezone

from flask_login import UserMixin
from werkzeug.security import check_password_hash, generate_password_hash

from extensions import db


def _ahora():
    """Fecha/hora actual en UTC (tz-aware)."""
    return datetime.now(timezone.utc)


class Paciente(db.Model):
    __tablename__ = "paciente"

    # El id NO es autoincremental: es el numero que se escribe en el panel
    # del exoesqueleto, asi que lo asigna el medico al dar de alta al paciente.
    id = db.Column(db.Integer, primary_key=True, autoincrement=False)
    nombre = db.Column(db.String(100), nullable=False)
    edad = db.Column(db.Integer, nullable=True)
    observaciones = db.Column(db.String(250), nullable=True)

    sesiones = db.relationship(
        "Sesion", backref="paciente", lazy=True, cascade="all, delete-orphan"
    )
    # Cuentas de usuario vinculadas a este paciente (rol 'paciente').
    usuarios = db.relationship("Usuario", backref="paciente", lazy=True)

    def resumen(self):
        """Dict minimo para la Pico (poca memoria)."""
        return {"id": self.id, "nombre": self.nombre}


class Usuario(UserMixin, db.Model):
    __tablename__ = "usuario"

    id = db.Column(db.Integer, primary_key=True)
    nombre_usuario = db.Column(db.String(50), unique=True, nullable=False)
    contrasena_hash = db.Column(db.String(255), nullable=False)
    rol = db.Column(db.String(20), nullable=False)  # 'medico' | 'paciente'
    # Solo para rol 'paciente': a que paciente puede ver.
    paciente_id = db.Column(
        db.Integer, db.ForeignKey("paciente.id"), nullable=True
    )

    def set_contrasena(self, contrasena: str):
        self.contrasena_hash = generate_password_hash(contrasena)

    def verificar_contrasena(self, contrasena: str) -> bool:
        return check_password_hash(self.contrasena_hash, contrasena)

    @property
    def es_medico(self) -> bool:
        return self.rol == "medico"

    def puede_ver_paciente(self, paciente_id: int) -> bool:
        """El medico ve a todos; el paciente solo a si mismo."""
        if self.es_medico:
            return True
        return self.paciente_id == paciente_id


class Sesion(db.Model):
    __tablename__ = "sesion"

    id = db.Column(db.Integer, primary_key=True)
    paciente_id = db.Column(
        db.Integer, db.ForeignKey("paciente.id"), nullable=False
    )
    # Identificador que manda la Pico para no duplicar envios.
    id_envio = db.Column(db.String(64), unique=True, nullable=False)
    brazo = db.Column(db.String(10), nullable=False)  # 'izquierdo' | 'derecho'
    # La fecha la pone el servidor al recibir (la Pico no tiene reloj).
    fecha = db.Column(db.DateTime(timezone=True), default=_ahora, nullable=False)
    rom_calibrado_grados = db.Column(db.Float, nullable=True)
    duracion_s = db.Column(db.Integer, nullable=True)
    # Cantidad de repeticiones de la sesion.
    repeticiones = db.Column(db.Integer, nullable=False, default=0)
    # Promedios calculados al guardar.
    rango_medio_grados = db.Column(db.Float, nullable=True)
    asistencia_media_pct = db.Column(db.Float, nullable=True)

    detalle = db.relationship(
        "Repeticion",
        backref="sesion",
        lazy=True,
        cascade="all, delete-orphan",
        order_by="Repeticion.numero",
    )

    def resumen(self):
        return {
            "id_sesion": self.id,
            "paciente_id": self.paciente_id,
            "brazo": self.brazo,
            "fecha": self.fecha.isoformat() if self.fecha else None,
            "rom_calibrado_grados": self.rom_calibrado_grados,
            "duracion_s": self.duracion_s,
            "repeticiones": self.repeticiones,
            "rango_medio_grados": self.rango_medio_grados,
            "asistencia_media_pct": self.asistencia_media_pct,
        }


class Repeticion(db.Model):
    __tablename__ = "repeticion"

    id = db.Column(db.Integer, primary_key=True)
    sesion_id = db.Column(db.Integer, db.ForeignKey("sesion.id"), nullable=False)
    numero = db.Column(db.Integer, nullable=False)
    rango_grados = db.Column(db.Float, nullable=False)  # max - min del angulo
    asistencia_pct = db.Column(db.Float, nullable=False)  # 0-100
    duracion_ms = db.Column(db.Integer, nullable=True)

    def resumen(self):
        return {
            "numero": self.numero,
            "rango_grados": self.rango_grados,
            "asistencia_pct": self.asistencia_pct,
            "duracion_ms": self.duracion_ms,
        }
