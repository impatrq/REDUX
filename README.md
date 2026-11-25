<div align="center">
<img src="Logotipo.png" alt="Logo proyecto REDUX" width="25%"/>
</div>

<div align="center">

# REDUX: Órtesis activa de codo para rehabilitación traumatológica

</div>

<div align="center">
<img src="WhatsApp Image 2026-11-25 at 15.00.30.jpeg" alt="Equipo y prototipo REDUX" width="75%"/>
</div>

## 🔗 Repositorio y Redes:

<a href="https://github.com/impatrq/REDUX" target="_blank">
  <img alt="GitHub" src="https://img.shields.io/badge/GitHub-REDUX-000?style=for-the-badge&logo=github&logoColor=white" />
</a>
<a href="https://trello.com/b/ddNpZCd9/kanban-2026721aredux" target="_blank">
  <img alt="Trello" src="https://img.shields.io/badge/Trello-Kanban-0052CC?style=for-the-badge&logo=trello&logoColor=white" />
</a>
<a href="https://www.instagram.com/Proyecto.Redux" target="_blank">
  <img alt="Instagram" src="https://img.shields.io/badge/Instagram-E4405F?style=for-the-badge&logo=instagram&logoColor=white" />
</a>

## 🗓️ Objetivo del Proyecto:

El nombre **REDUX** proviene del latín y significa *"restauración"* o *"retorno"*. El objetivo general del proyecto es desarrollar un prototipo funcional, seguro y de costo accesible de órtesis activa de codo que asista los movimientos de flexión y extensión a partir de la señal electromiográfica de superficie (sEMG) del bíceps y del tríceps, con registro digital de cada sesión para su uso bajo supervisión profesional en consultorios de kinesiología.

## 🌎 Visión a futuro e Impacto esperado

REDUX busca ser una herramienta de apoyo para el kinesiólogo, no un reemplazo. Al asistir según el esfuerzo muscular real del paciente, reduce la fatiga, evita compensaciones musculares perjudiciales y permite registrar de forma objetiva la evolución (rango de movimiento y porcentaje de asistencia sesión a sesión). A futuro, la arquitectura abierta y accesible permite proyectar mejoras como control de posición, rutinas preprogramadas, extensión a otras articulaciones y ensayos con usuarios.

## 💭 ¿De dónde surgió la idea?

Los procesos de rehabilitación traumatológica del miembro superior (fracturas, luxaciones, traumatismos o cirugías) requieren numerosas sesiones repetitivas donde el profesional guía manualmente el brazo y registra mediciones puntuales, mientras que los exoesqueletos robóticos comerciales tienen costos elevados que dificultan su adopción en consultorios y centros de nuestra región. En el marco de las Prácticas Profesionalizantes de la especialidad Aviónica de la **E.E.S.T. N°7 "Taller Regional Quilmes" (IMPA)**, el equipo desarrolló una alternativa integral, segura y accesible.

## 🖐 Descripción del proyecto:

REDUX se compone de cuatro subsistemas físicos y una plataforma web remota:

* **Órtesis:** estructura impresa en 3D (diseño preliminar en Onshape y definitivo en AutoCAD) fijada a brazo y antebrazo mediante tiras elásticas y velcro, con topes mecánicos rígidos en la articulación y un encoder magnético absoluto **AS5600** (`I2C` a 400 kHz, 12 bits) que mide el ángulo articular.
* **Unidad de accionamiento:** motor de corriente continua de 12 V con reductora (30 RPM) montado externamente para reducir el peso en el brazo y aislar el ruido eléctrico, transmitiendo fuerza mediante un carrete y un sistema de poleas.
* **Unidad electrónica:** centrada en una **Raspberry Pi Pico 2 W (RP2350)** con firmware en **C11** sobre **Pico SDK 2.x** y lazo determinista de 1 ms (Núcleo 0 dedicado al control y Núcleo 1 a la interfaz). Integra dos cadenas analógicas de acondicionamiento sEMG (preamplificación con **AD620**, filtros activos Sallen-Key pasa-altos ~20 Hz y pasa-bajos ~500 Hz con **TL084**, rectificación de precisión de onda completa y circuito **RLD** con resistencia de seguridad de 4,7 a 10 MΩ), etapa de potencia con puente H **BTS7960** (PWM de 20 kHz) y aislación galvánica mediante optoacopladores **6N137**, separación de masas (`GND1` potencia y `GND2` lógica/señal) y fuentes aisladas (**LM2596** y **B0505S**).
* **Panel de control:** soporte inclinado a 35° con pantalla **TFT ST7735** (1,44", 128x128, `SPI1`) programada con **LVGL 8.3.11** (diseñada en SquareLine Studio) y teclado matricial de membrana **4x4**, comunicándose mediante colas para calibrar el rango articular y los umbrales de reposo/MVC en cada sesión, iniciar/finalizar sesiones y visualizar fallas o telemetría.
* **Plataforma web y gestión de datos:** aplicación **Flask** con **SQLite** y **SQLAlchemy**, interfaz en **HTML**, **Bootstrap 5** y **Chart.js**, autenticación con roles (`médico/kinesiólogo` y `paciente`), envío HTTP autenticado (`X-Redux-Key`) con `id_envio` idempotente y pruebas automáticas con `pytest`.

En operación, la lógica de intención evalúa la activación muscular mediante compuerta **XOR** entre bíceps y tríceps: asiste flexión o extensión de forma proporcional al esfuerzo y detiene/frena el motor ante reposo, co-contracción, límites articulares o fallas detectadas por la máquina de estados y el watchdog de 500 ms.

## 🛠️ Componentes principales:

| Componente | Función / Especificación |
| :--- | :--- |
| **Raspberry Pi Pico 2 W (RP2350)** | Microcontrolador principal (Cortex-M33, Wi-Fi, ADC interno, PWM, I2C, SPI) |
| **Encoder magnético AS5600** | Medición angular absoluta sin contacto de 12 bits vía I2C (0x36) |
| **Amplificadores de instrumentación AD620** | Preamplificación y ganancia diferencial de señales sEMG |
| **Amplificadores operacionales TL084** | Filtros activos pasa-altos/pasa-bajos, rectificador de precisión y RLD |
| **Electrodos descartables Ag/AgCl** | Adquisición superficial diferencial en bíceps y tríceps + referencia RLD |
| **Puente H BTS7960** | Control de motor DC 12 V (30 RPM) mediante señales PWM de 20 kHz |
| **Optoacopladores 6N137** | Aislación de señales PWM entre etapas de lógica y potencia |
| **Convertidores de alimentación / aislación** | LM2596, convertidor aislado B0505S y etapas ±5 V para analógica |
| **Pantalla TFT ST7735 + Teclado 4x4** | Interfaz local de calibración, control de sesión y diagnóstico |
| **Piezas impresas en 3D** | Estructura de órtesis, soporte de motor, carrete, caja electrónica y panel |

## 🧑🏽‍💻 Integrantes

| Integrantes | Responsabilidades | 
| :--- | :--- | 
| Bajales Ignacio | Gestion de Redes |
| Ramirez Hillz Benicio| Desarrollo de software |  
| Relañez Joaquin| Diseño de circuitos electronicos | 
| Gomez Facundo| Diseño de Placas electronicas | 
| Strangio Gael| Diseño 3D de eswtructura | 

## 🤝 Colaboración y Agradecimientos:

Este prototipo forma parte de las Prácticas Profesionalizantes de la especialidad Aviónica de la **E.E.S.T. N°7 "Taller Regional Quilmes" (IMPA)**. Agradecemos especialmente a nuestro sponsor, la **Universidad Argentina de la Empresa (UADE)**, por el asesoramiento, el espacio de trabajo y la impresión 3D de la estructura de la órtesis, así como a los docentes tutores y a toda la comunidad educativa.

Si deseas colaborar con **REDUX**, puedes aportar en:

- Perfeccionamiento del código y validaciones clínicas/técnicas.
- Mejora de hardware, PCB SMD y fabricación mecánica.
- Documentación técnica, manuales y ensayos de laboratorio.
- Apoyo institucional o financiero para investigación y desarrollo.

## 📱 Contactanos:

<a href="https://mail.google.com/mail/?view=cm&to=" target="_blank">
    <img alt="Gmail" src="https://img.shields.io/badge/Gmail-D14836?style=for-the-badge&logo=gmail&logoColor=white" />
</a>
<a href="https://www.instagram.com/Proyecto.Redux" target="_blank">
    <img alt="Instagram" width="123px" src="https://img.shields.io/badge/Instagram-E4405F?style=for-the-badge&logo=instagram&logoColor=white" />
</a>

## 📱 Contactos personales

| Integrante | Gmail | LinkedIn |
| :--- | :--- | :--- |
| ** ** | | |
| ** ** | | |
| ** ** | | |
| ** ** | | |
| ** ** | | |

---

![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg) ![Status](https://img.shields.io/badge/status-active-brightgreen)
```[cite: 1, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29]
