#----------- AUTORES -----------
# Jeshua Isaac Rocha Sainez
# Fabricio Becerra Quezada
# Laura Sofia Ornelas Valenzuela
#-------------------------------

# ===============================
# IMPORTACIÓN DE LIBRERÍAS
# ===============================
import json                     # Manejo de datos JSON
import boto3                    # SDK de AWS para Python
from decimal import Decimal     # Manejo de números decimales de DynamoDB
import base64                   # Decodificación de imágenes en Base64
import time                     # Manejo de tiempo para timestamps

# ===============================
# INICIALIZACIÓN DE CLIENTES AWS
# ===============================
# Se inicializan fuera del handler para mejorar rendimiento
dynamodb = boto3.resource('dynamodb')  # Recurso DynamoDB
s3 = boto3.client('s3')                # Cliente S3

# ===============================
# CONSTANTES DE CONFIGURACIÓN
# ===============================
TABLE_NAME = 'Datos_MPU'        # Nombre de la tabla DynamoDB
DeviceId = 'DRON_ESP32'         # ID del dispositivo que envía datos
BUCKET = 'robot-submarino'      # Bucket S3 donde se guardan las imágenes

# ===============================
# HEADERS CORS PARA RESPUESTAS HTTP
# ===============================
CORS_HEADERS = {
    "Access-Control-Allow-Origin": "*",
    "Access-Control-Allow-Methods": "GET, OPTIONS, POST",
    "Access-Control-Allow-Headers": "Content-Type,X-Amz-Date,Authorization,X-Api-Key,X-Amz-Security-Token",
    "Content-Type": "application/json"
}

# Headers específicos para imágenes
IMAGE_HEADERS = {
    **CORS_HEADERS,
    "Content-Type": "image/jpeg",
    "Cache-Control": "no-cache, no-store, must-revalidate"
}

# ===============================
# FUNCIÓN PARA CONVERTIR DECIMALES
# ===============================
# Convierte objetos Decimal de DynamoDB a int o float
# para poder serializarlos a JSON
def decimal_encoder(obj):
    if isinstance(obj, Decimal):
        if obj % 1 == 0:
            return int(obj)
        else:
            return float(obj)
    raise TypeError(f"Objeto de tipo {type(obj)._name_} no es serializable")

# ===============================
# FUNCIÓN PRINCIPAL LAMBDA
# ===============================
def lambda_handler(event, context):
    # Obtiene la ruta y el método HTTP
    path = event.get("path") or ""
    http_method = event.get("httpMethod", "")

    # Logs para depuración
    print("METHOD:", http_method)
    print("PATH:", path)

    # ===============================
    # ENDPOINT /data (GET)
    # ===============================
    # Obtiene los últimos 10 registros del dispositivo desde DynamoDB
    if http_method == "GET" and path.endswith("/data"):
        try:
            table = dynamodb.Table(TABLE_NAME)
            response = table.query(
                KeyConditionExpression='DeviceId = :val',
                ExpressionAttributeValues={':val': DeviceId},
                ScanIndexForward=False,
                Limit=10
            )
            return {
                'statusCode': 200,
                'headers': CORS_HEADERS,
                'body': json.dumps(response['Items'], default=decimal_encoder)
            }
        except Exception as ex:
            print(f"Error al leer DynamoDB: {str(ex)}")
            return {
                'statusCode': 500,
                'headers': CORS_HEADERS,
                'body': json.dumps({'error': str(ex)})
            }

    # ===============================
    # ENDPOINT /capture
    # ===============================
    elif path.endswith("/capture"):

        # -------------------------------
        # MÉTODO POST
        # -------------------------------
        # Recibe una imagen en Base64 desde el ESP32,
        # la guarda en S3 y devuelve una URL firmada
        if http_method == "POST":
            try:
                body = event.get('body', '')
                if not body:
                    raise ValueError("No se recibió body")

                # Parsear JSON recibido
                data = json.loads(body)
                base64_str = data.get('image')
                if not base64_str:
                    raise ValueError("No se encontró 'image' en JSON")

                # Decodificación de la imagen
                image_bytes = base64.b64decode(base64_str)
                print(f"DEBUG - Foto decodificada, tamaño: {len(image_bytes)} bytes")

                # Guardar imagen en S3 dentro de la carpeta captures/
                KEY = f"captures/capture_{int(time.time())}.jpg"
                s3.put_object(
                    Bucket=BUCKET,
                    Key=KEY,
                    Body=image_bytes,
                    ContentType='image/jpeg'
                )
                print(f"DEBUG - Foto guardada en S3: {KEY}")

                # Generar URL firmada temporal
                signed_url = s3.generate_presigned_url(
                    'get_object',
                    Params={'Bucket': BUCKET, 'Key': KEY},
                    ExpiresIn=3600  # 1 hora
                )

                return {
                    'statusCode': 200,
                    'headers': CORS_HEADERS,
                    'body': json.dumps({
                        'message': 'Foto recibida y guardada en captures/',
                        'url': signed_url,
                        'size': len(image_bytes)
                    })
                }

            except Exception as e:
                print(f"ERROR en POST /capture: {str(e)}")
                import traceback
                print("TRACEBACK completo:")
                print(traceback.format_exc())
                return {
                    'statusCode': 500,
                    'headers': CORS_HEADERS,
                    'body': json.dumps({'error': str(e)})
                }

        # -------------------------------
        # MÉTODO GET
        # -------------------------------
        # Informa que el endpoint espera un POST
        elif http_method == "GET":
            return {
                'statusCode': 200,
                'headers': CORS_HEADERS,
                'body': json.dumps({'message': 'Usa POST para enviar foto y obtener URL de S3'})
            }

        # -------------------------------
        # MÉTODO NO PERMITIDO
        # -------------------------------
        else:
            return {
                'statusCode': 405,
                'headers': CORS_HEADERS,
                'body': json.dumps({'error': 'Método no permitido'})
            }

    # ===============================
    # ENDPOINT NO EXISTENTE
    # ===============================
    else:
        return {
            'statusCode': 404,
            'headers': CORS_HEADERS,
            'body': json.dumps({'error': 'Endpoint no encontrado'})
        }
    
