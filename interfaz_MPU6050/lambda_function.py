import json
import boto3

# Inicializa el cliente de DynamoDB fuera del handler (mejor rendimiento)
dynamodb = boto3.resource('dynamodb')
TABLE_NAME = 'Datos_MPU' # Nombre de la tabla
DEVICE_ID = 'MPU6500_ESP32' # ID del dispositivo

# Función Lambda para leer los últimos 10 registros de DynamoDB
def lambda_handler(event, context):
    try:
        table = dynamodb.Table(TABLE_NAME)
        response = table.query(
            KeyConditionExpression='DEVICE_ID = :val',
            ExpressionAttributeValues={':val': DEVICE_ID},
            ScanIndexForward=False, # Orden descendente
            Limit=10 # Limita a 1 el resultado
        )

        items = response['Items']
        return {
            'statusCode': 200,
            'headers': {
                'Access-Control-Allow-Origin': '*',
                'Content-Type': 'application/json'
            },
            'body': json.dumps(items)
        }

    except Exception as ex:
        print(f"Error al leer DynamoDB: {str(ex)}")
        return {
            'statusCode': 500,
            'body': json.dumps({'error': str(ex)})
        }
    
