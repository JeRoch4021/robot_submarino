#include <Wire.h>
#include <MPU6050.h>

MPU6050 mpu;

void setup() {
  Serial.begin(115200); // Iniciamos el puerto serial
  Wire.begin(21, 22); // Iniciamos I2C
  Serial.println("Inicializando MPU6050...");
  mpu.initialize(); // Iniciamos el sensor

  if (!mpu.testConnection()) {
    Serial.println("MPU6050 no conectado");
    while (true);
  }
  Serial.println("Conexion exitosa con MPU6050...");
}

void loop() {
  int16_t ax, ay, az;
  int16_t gx, gy, gz;

  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
  
  // Formato CSV
  Serial.print(ax); Serial.print(",");
  Serial.print(ay); Serial.print(",");
  Serial.print(az); Serial.print(",");
  Serial.print(gx); Serial.print(",");
  Serial.print(gy); Serial.print(",");
  Serial.print(gz);

  Serial.println();

  delay(20); // 20 ms equivalente a 50 Hz
}
