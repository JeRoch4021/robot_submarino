#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <MPU6050.h>
#include <ArduinoJson.h>
#include "SPIFFS.h"

// --- LIBRERIAS Y DEFINICIONES PARA DHT ---
#include <Adafruit_Sensor.h>
#include <DHT.h>

// Parametros de Red-WiFi
const char* ssid = "rochasainez";
const char* password = "35631354";

// Parametros de MQTT
const char* mqtt_server = "a2unzqzb4y72ui-ats.iot.us-east-2.amazonaws.com";
const int mqtt_port = 8883;
const char* TOPIC_PUB = "mpu/data";
const char* THING_NAME = "MPU6500_ESP32";

const char* ca_cert_path = "/AmazonRootCA1.pem";
const char* client_cert_path = "/a1752b5-certificate.pem.crt";
const char* private_key_path = "/a1752b5-private.pem.key";

String Read_ca_cert;
String Read_client_cert;
String Read_private_key;

WiFiClientSecure espClient;
PubSubClient client(espClient);

MPU6050 mpu;

// Sensor DHT
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht (DHTPIN, DHTTYPE);

String leerArchivos (fs::FS &fs, const char * path) {
  Serial.printf("Leyendo archivos: %s\n", path);
  File file = fs.open(path, "r");
  if (!file || file.isDirectory()) {
    Serial.println("No se pudo abrir el archivo para leerlo");
    return String();
  }

  String fileContent = file.readString();
  file.close();
  Serial.printf("Archivo leído: %d bytes\n", fileContent.length());
  return fileContent;
}

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Conectando...");
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  randomSeed(micros());
  
  Serial.println("");
  Serial.println("\nWiFi conectado.");
  Serial.println("\nDirección ip: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  
}

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


void setup() {
  Serial.begin(115200); // Iniciamos el puerto serial
  Serial.println("--- INICIO DE PROGRAMA ---");
  setup_wifi();

  if (!SPIFFS.begin(true)) {
    Serial.println("Se ha producido un error al montar SPIFFS");
    while(true);
  }
  Serial.println("SPIFFS montando correctamente.");

  Read_ca_cert = leerArchivos(SPIFFS, ca_cert_path);
  Read_client_cert = leerArchivos(SPIFFS, client_cert_path);
  Read_private_key = leerArchivos(SPIFFS, private_key_path);

  if (Read_ca_cert.length() == 0 || Read_client_cert.length() == 0 || Read_private_key.length() == 0) {
    Serial.println("Error: Uno o mas archivos de credenciales estan vacios o no se encontraron");
    Serial.println("Asegurate de haber subido los archivos .pem al ESP32 usando la herramienta de carga SPIFFS");
    while(true);
  }
  
  espClient.setCACert(Read_ca_cert.c_str());
  espClient.setCertificate(Read_client_cert.c_str());
  espClient.setPrivateKey(Read_private_key.c_str());

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  
  Wire.begin(21, 22); // Iniciamos I2C
  Serial.println("Inicializando MPU6050...");
  mpu.initialize(); // Iniciamos el sensor

  if (!mpu.testConnection()) {
    Serial.println("MPU6050 no conectado");
    while (true);
  }
  Serial.println("Conexion exitosa con MPU6050...");

  dht.begin();
  Serial.printf("Sensor DHT (%s) inicializado en GPIO %d. \n", (DHTTYPE == DHT11 ? "DHT11" : "DHT22"), DHTPIN);
  
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  
  int16_t ax, ay, az;
  int16_t gx, gy, gz;

  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

  float humedad = dht.readHumidity();
  float temperatura = dht.readTemperature();

  if (isnan(humedad) || isnan(temperatura)) {
    Serial.print("Fallo en lectura del sensor DHT. Saltar iteracion.");
    delay(1000);
    return;
  }

  StaticJsonDocument<512> doc;
  doc["AccelX"] = ax;
  doc["AccelY"] = ay;
  doc["AccelZ"] = az;
  doc["GyroX"] = gx;
  doc["GyroY"] = gy;
  doc["GyroZ"] = gz;

  // Datos DHT
  doc["Temperatura"] = temperatura;
  doc["Humedad"] = humedad;

  Serial.println();
  
  // Impresion de los datos del sensor MPU en formato CSV
  Serial.print(ax); Serial.print(",");
  Serial.print(ay); Serial.print(",");
  Serial.print(az); Serial.print(",");
  Serial.print(gx); Serial.print(",");
  Serial.print(gy); Serial.print(",");
  Serial.print(gz);

  Serial.println();

  // Impresion de los datos del sensor DHT en formato CSV
  Serial.printf("Temperatura: %.2f C\n", temperatura);
  Serial.printf("Humedad: %.2f %% \n", humedad);

  Serial.println();

  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer);

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

  delay(1000); // 20 ms equivalente a 50 Hz
}
