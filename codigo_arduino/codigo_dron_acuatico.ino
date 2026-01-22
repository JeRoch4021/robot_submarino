/*
Autores
  Rocha Sainez Jeshua Isaac
  Becerra Quezada Fabricio
*/
// Implementación de librerías para el desarrollo de este proyecto.
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <MPU6050.h>
#include <ArduinoJson.h>
#include "SPIFFS.h"
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <Arduino.h>
#include <CQRobotTDS.h>

// Parámetros de Red-WiFi
const char* ssid = "ProfesoresTecNM-UF";
const char* password = "T3cNML30n@2024$.";

// Parámetros de MQTT para AWS.
const char* mqtt_server = "a2unzqzb4y72ui-ats.iot.us-east-2.amazonaws.com";
const int mqtt_port = 8883;
const char* TOPIC_PUB = "dron/data";
const char* THING_NAME = "DRON_ESP32";

// Certificados de autenticación de AWS.
const char* ca_cert_path = "/AmazonRootCA1.pem";
const char* client_cert_path = "/a1752b5-certificate.pem.crt";
const char* private_key_path = "/a1752b5-private.pem.key";

// Variables de lectura para los certificados de autenticación de AWS.
String Read_ca_cert;
String Read_client_cert;
String Read_private_key;

// Instrucciones encargadas de crear el túnel seguro y el cliente MQTT 
// que llevará los datos a AWS.
WiFiClientSecure espClient;
PubSubClient client(espClient);

// Configuración del sensor TDS (Salinidad).
# define TDS_PIN 34
CQRobotTDS tds(TDS_PIN, 3.3);
float ppm = 0.0; // Partes por millón

// Configuración del sensor MPU6050.
MPU6050 mpu;

// Configuración del sensor DHT (temperatura y humedad).
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht (DHTPIN, DHTTYPE);

// Función que permite gestionar la memoria interna del ESP32,
// su propósito es entrar al sistema de archivos del ESP32 (SPIFFS)
// para extraer los certificados de seguridad que necesitan para conectarse
// a AWS.
String leerArchivos (fs::FS &fs, const char * path) {
  Serial.printf("Leyendo archivos: %s\n", path);
  File file = fs.open(path, "r");
  // Si el archivo existe o si es una carpeta.
  if (!file || file.isDirectory()) {
    Serial.println("No se pudo abrir el archivo para leerlo");
    return String();
  }
  // Tomar el texto largo y complejo que hay dentro del certificado.
  String fileContent = file.readString();
  file.close();
  Serial.printf("Archivo leído: %d bytes\n", fileContent.length());
  return fileContent;
}

// Función que permite la puerta de entrada a internet para el
// el ESP32 y tener la capacidad de enviar datos a la nube de
// AWS de manera inalámbrica.
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Conectando...");

  // Buscar el nombre de la red y usar la contraseña a esa red.
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  // Cifrado de los datos de manera segura e impredecible,
  // vital para el protocolo de internet.
  randomSeed(micros());
  
  Serial.println("");
  Serial.println("\nWiFi conectado.");
  Serial.println("\nDirección ip: ");
  Serial.println(WiFi.localIP());
}

// Función que permite establecer un buzón de entrada
// para recibir mensajes desde AWS hacia el dron, aún actualmente
// solo se está enviando datos, no recibiendo órdenes.
void callback(char* topic, byte* payload, unsigned int length) {
  
}

// Función que permite crear un mecanismo de supervivencia del sistema,
// verificando constantemente si la conexión con AWS sigue viva, si se pierde
// el sistema entra en modo emergencia e intenta reconectarse.
void reconnect() {
  while (!client.connected()) {
    Serial.print("Conectando a AWS IoT...");
    
    if (client.connect(THING_NAME)) {
      Serial.println("Conectado!");
    } else {
      Serial.print("Error: ");
      Serial.print(client.state());
      Serial.print(" Intentando de nuevo en 2 segundos\n");
      delay(2000);
    }
  }
}

// Esta función es el encargado de configurar el inicio del proyecto,
// preparando todo el hardware y las credenciales antes de que el dron
// empiece a trabajar en bucle.
void setup() {
  // Iniciamos el puerto serial para ver mensajes en el monitor serie.
  Serial.begin(115200);
  Serial.println("--- INICIO DE PROGRAMA ---");
  // Llamamos a la función para conectar el dispositivo a internet.
  setup_wifi();

  // Montar el sistema de archivos internos del ESP32.
  if (!SPIFFS.begin(true)) {
    Serial.println("Se ha producido un error al montar SPIFFS");
    while(true);
  }
  Serial.println("SPIFFS montando correctamente.");

  // Credenciales digitales para guardar las llaves leídas en la memoria interna
  // y entregárselas al cliente seguro. 
  Read_ca_cert = leerArchivos(SPIFFS, ca_cert_path);
  Read_client_cert = leerArchivos(SPIFFS, client_cert_path);
  Read_private_key = leerArchivos(SPIFFS, private_key_path);

  // Verifica el contenido de las credenciales digitales, evitando que se lean archivos vacios.
  if (Read_ca_cert.length() == 0 || Read_client_cert.length() == 0 || Read_private_key.length() == 0) {
    Serial.println("Error: Uno o mas archivos de credenciales estan vacios o no se encontraron");
    Serial.println("Asegurate de haber subido los archivos .pem al ESP32 usando la herramienta de carga SPIFFS");
    while(true);
  }

  // Configuración de la capa de seguridad para instalar las llaves
  // dentro del motor de seguridad del ESP32.
  espClient.setCACert(Read_ca_cert.c_str());
  espClient.setCertificate(Read_client_cert.c_str());
  espClient.setPrivateKey(Read_private_key.c_str());

  // Le dice al mensajero MQTT cual es la dirección exacta
  // de tu instancia de AWS en la nube.
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  // Activa el protocolo I2C en los pines específicos para poder hablar
  // con el sensor de movimiento MPU. 
  Wire.begin(23, 22);
  Serial.println("Inicializando MPU6050...");
  mpu.initialize(); // Iniciamos el sensor

  // Verificamos la conexión del sensor para evitar enviar datos basura.
  if (!mpu.testConnection()) {
    Serial.println("MPU6050 no conectado");
    while (true);
  }
  Serial.println("Conexion exitosa con MPU6050...");

  // Iniciamos el sensor de temperatura y humedad ambiental.
  dht.begin();
  Serial.printf("Sensor DHT (%s) inicializado en GPIO %d. \n", (DHTTYPE == DHT11 ? "DHT11" : "DHT22"), DHTPIN);

  Serial.println("--- Sistema Completo Inicializado ---");
}

void loop() {
  // Revisar si la conexión MQTT hacia AWS sigue conectado,
  // si se rompio llama a reconnect
  if (!client.connected()) {
    reconnect();
  }
  // Mantiene activa la comunicación, procesa mensajes y asegura 
  // que el servidor no crea que el ESP32 se apagó.
  client.loop();

  // Inicialización de variables del sensor MPU6050
  int16_t ax, ay, az;
  int16_t gx, gy, gz;

  // Pedirle al MPU toda su información.
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

  // Obtenemos los datos del sensor DHT.
  float humedadDHT = dht.readHumidity();
  float temperaturaDHT = dht.readTemperature();

  // Lectura TDS (Usamos la temperatura ambiente para compensar si el sensor no tiene sonda)
  if (isnan(temperaturaDHT)) temperaturaDHT = 25.0; 
  ppm = tds.update(temperaturaDHT);

  // Detenemos temporalmente el sistema si los datos del sensor DHT están vacíos.
  if (isnan(humedadDHT) || isnan(temperaturaDHT)) {
    Serial.print("Fallo en lectura del sensor DHT. Saltar iteracion.");
    delay(1000);
    return;
  }

  // Empaquetado de los datos en un formato JSON para que AWS  
  // pueda entenderlos fácilmente.
  StaticJsonDocument<512> doc;
  // Datos del sensor MPU.
  doc["AccelX"] = ax;
  doc["AccelY"] = ay;
  doc["AccelZ"] = az;
  doc["GyroX"] = gx;
  doc["GyroY"] = gy;
  doc["GyroZ"] = gz;

  // Datos del sensor DHT.
  doc["Temperatura"] = temperaturaDHT;
  doc["Humedad"] = humedadDHT;

  // Datos del sensor de salinidad (TDS analógico).
  doc["salinidad"] = ppm;

  // Reservamos un espacio en la memoria RAM del ESP32 (un buffer)
  // capaz de guardar hasta 1024 caracteres.
  char jsonBuffer[512];
  // Toma el objeto doc (estructura interna de la librería ArduinoJson) 
  // y los convierte en una cadena de texto plano dentro de jsonBuffer.
  serializeJson(doc, jsonBuffer);

  // Le dice al mensajero MQTT que tome el texto de jsonBuffer y lo publique al tema
  // que definimos como "dron/data".
  if (client.publish(TOPIC_PUB, jsonBuffer)) {
    Serial.print("Publicando en ");
    Serial.print(TOPIC_PUB);
    Serial.print(": ");
    Serial.println(jsonBuffer);
  } else {
    Serial.println("Fallo la Publicación MQTT (Error: ");
    Serial.print(client.state());
    Serial.println(")");
  }

  Serial.println();
  
  // Impresión de los datos del sensor MPU en formato CSV (Valores Separados por Comas).
  Serial.print(ax); Serial.print(",");
  Serial.print(ay); Serial.print(",");
  Serial.print(az); Serial.print(",");
  Serial.print(gx); Serial.print(",");
  Serial.print(gy); Serial.print(",");
  Serial.print(gz);

  Serial.println();

  // Impresión de los datos del sensor DHT en formato CSV (Valores Separados por Comas).
  Serial.printf("Temperatura: %.2f C\n", temperaturaDHT);
  Serial.printf("Humedad: %.2f %% \n", humedadDHT);

  Serial.println();

  // Impresión de los datos del sensor de salinidad (TDS analógico) en formato 
  // CSV (Valores Separados por Comas).
  Serial.printf("\nSalinidad: %.2f ppm | T.Ref: %.2f C", ppm, temperaturaDHT);
    
  Serial.println();

  // Controlamos el ritmo de frecuencia con la que tu ESP32 realiza todas las 
  // tareas del loop().
  delay(50); // 50 equivale a una frecuencia de 20 Hz (20 veces por segundo).
}
