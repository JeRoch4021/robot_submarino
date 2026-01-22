//Christian Rafael Landeros Portillo
//Laura Sofia Ornelas Valenzuela
#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <base64.h>

// ===========================
// Select camera model in board_config.h
// ===========================
#include "board_config.h"

// ===========================
// Enter your WiFi credentials
// ===========================
const char* ssid = "RED";
const char* password = "CONTRASEÑA";

// URL de tu API Gateway
const char* api_url = "https://4i25h1owl2.execute-api.us-east-2.amazonaws.com/v1/capture";

// Intervalo de envío automático (en milisegundos)
unsigned long lastCaptureTime = 0;
const unsigned long captureInterval = 10000;  // 10 segundos - ajusta según necesites

void startCameraServer();
void setupLedFlash();

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_UXGA;
  config.pixel_format = PIXFORMAT_JPEG;  // for streaming
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  if (config.pixel_format == PIXFORMAT_JPEG) {
    if (psramFound()) {
      config.jpeg_quality = 10;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  } else {
    config.frame_size = FRAMESIZE_240X240;
#if CONFIG_IDF_TARGET_ESP32S3
    config.fb_count = 2;
#endif
  }

  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t *s = esp_camera_sensor_get();
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);
    s->set_brightness(s, 1);
    s->set_saturation(s, -2);
  }

  if (config.pixel_format == PIXFORMAT_JPEG) {
    s->set_framesize(s, FRAMESIZE_QVGA);
  }

  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);

#if defined(LED_GPIO_NUM)
  setupLedFlash();
#endif

  WiFi.begin(ssid, password);
  WiFi.setSleep(false);

  Serial.print("WiFi connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  startCameraServer();

  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");

  // Inicializar el tiempo para el primer envío
  lastCaptureTime = millis();
}

void sendPhotoToAPI() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("ERROR: Fallo al capturar frame");
    return;
  }

  Serial.printf("Foto capturada - Tamaño: %u bytes\n", fb->len);

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(api_url);
    http.addHeader("Content-Type", "application/json");  // Cambia a JSON

    // Convierte el buffer JPEG a base64
    String base64Image = base64::encode(fb->buf, fb->len);  // Necesitas #include <base64.h> o una lib base64

    // Payload JSON simple
    String payload = "{\"image\": \"" + base64Image + "\"}";

    Serial.printf("Enviando POST (base64, longitud payload: %u)\n", payload.length());
    int httpCode = http.POST(payload);

    if (httpCode > 0) {
      Serial.printf("Código HTTP: %d\n", httpCode);
      String response = http.getString();
      Serial.println("Respuesta de API: " + response);
    } else {
      Serial.printf("Error en POST: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  } else {
    Serial.println("WiFi no conectado");
  }

  esp_camera_fb_return(fb);
}

void loop() {
  unsigned long currentTime = millis();

  if (currentTime - lastCaptureTime >= captureInterval) {
    lastCaptureTime = currentTime;
    sendPhotoToAPI();
  }

  delay(100);  // Pequeño delay para no saturar el CPU
}
