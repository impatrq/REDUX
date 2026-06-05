#Importamos las librerias de Flask para el servidor
#Importamos Jsonify para transformar diccionarios de Python a formato JSON
# 'request' nos permite capturar y leer los datos que entran al servidor
from flask import Flask, jsonify, request

#iniciamos flask y le establecemos un nombre
app = Flask(__name__)

# definimos una ruta ('/') y la configuramos para que acepte GET y POST
@app.route('/', methods=['GET', 'POST'])
def root():

    # nos fijamos si el dispositivo que se conectó está enviando información [POST]
    if request.method == 'POST':
        #Extraemos los datos del archivo JSON y los guardamos en la variable "datos"
        datos = request.get_json()
        #Mostramos los datos recibidos en la terminal
        print("\n--- DATOS RECIBIDOS ---")
        print(datos)
        print("------------------------\n")
        
        # Le respondemos a la Pico que todo salió bien y muestro los datos que llegaron
        #El número 201 es el código HTTP estándar para confirmar que se "Creó/Recibió" la información
        return jsonify({"status": "recibido", "mensaje": "Datos de la Pico guardados", "datos guardados": datos}), 201
    
    # Si la petición no es un POST (es decir, entrás a la web común de la PC o celular),
    # el servidor interpreta que es un GET y simplemente muestra este texto en pantalla para mostrar que esta conectado.
    return "Redux_API"
    

#--------------------------
# GET --> obtener info
# POST --> crear info
# PUT --> actualizar info
# DELETE --> borrar info
#---------------------------

# ----ARRANQUE DEL SERVIDOR----
# Esta condición asegura que el servidor se encienda únicamente si ejecutamos este archivo directamente
if __name__ == '__main__':
    # host='0.0.0.0': Abre los puertos de la PC para escuchar a cualquier dispositivo en la red local en nuestro caso la Pi PIco.
    # port=8000: Define el puerto de comunicación.
    # debug=True: Si modificás el código y guardás, el servidor se reinicia solo sin tener que cerrarlo.
    app.run(host='0.0.0.0', port=8000, debug=True)