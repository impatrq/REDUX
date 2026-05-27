import network
import urequests
import time
import ujson

wlan = network.WLAN(network.STA_IF)
wlan.active(True)
wlan.connect('Cooperadora Profesores', 'Profes_IMPA_2022')

while not wlan.isconnected():
    print('Connecting to WiFi...')
    time.sleep(1)
print('Connected to WiFi', wlan.ifconfig())  

datos_pruba = {
    "dispositivo_id": "La polla en vinagre",
    "Dato_1": 69.5,
    "Dato_2": 67.2,
    "Pensamientos finales": "Me cago en la puta"
}

url = "http://192.168.111.191:8000"
headers = {'Content-Type': 'application/json'}

json_data = ujson.dumps(datos_pruba)

try:
    # Enviar el paquete JSON mediante un HTTP POST
    print("Enviando datos...")
    respuesta = urequests.post(url, data=json_data, headers=headers)
    
    print("Código de respuesta del servidor:", respuesta.status_code)
    print("Contenido:", respuesta.text)
    
    respuesta.close() 
except Exception as e:
    print("Error al enviar los datos:", e)