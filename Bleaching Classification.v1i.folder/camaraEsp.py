"""
Autor:
    Landeros Portillo Christian Rafael
"""
import cv2
from ultralytics import YOLO
import os
import numpy as np
import requests
import time

# ===================== CONFIGURACIÓN =====================
ESP32_IP = "192.168.64.74"
STREAM_URL = f"http://{ESP32_IP}/capture"   # Ruta de captura individual de fotos

# ===================== CARGA DEL MODELO =====================
model_path = os.path.join(os.getcwd(), "runs", "classify", "train2", "weights", "best.pt")

# Verificar que el modelo existe
if not os.path.exists(model_path):
    print(f"ERROR: No se encontró el modelo en:\n{model_path}")
    exit()

# Cargar el modelo YOLOv8
print(f"Modelo encontrado: {model_path}")
model = YOLO(model_path)

# ===================== INICIO DEL STREAM =====================
print(f"Conectando a la cámara ESP32 vía /capture... {STREAM_URL}")
print("Presiona Q para salir\n")

# Configurar ventana OpenCV
cv2.namedWindow("Clasificador de Corales (ESP32) - Presiona Q", cv2.WINDOW_NORMAL)
cv2.resizeWindow("Clasificador de Corales (ESP32) - Presiona Q", 400, 400)

# ========================================================
#    GENERADOR QUE PIDE FOTOS INDIVIDUALES CADA CIERTO TIEMPO
# ========================================================
def get_frames_from_capture(url, interval=0.002):
    """
    Generador que pide una foto nueva cada 'interval' segundos.
    Simula un stream lento pero estable con la ruta /capture.
    """
    while True:
        try:
            print(f"Pidiendo nueva foto... ({time.strftime('%H:%M:%S')})")
            r = requests.get(url, timeout=5)
            r.raise_for_status()

            # Convertir respuesta (JPEG puro) a frame OpenCV
            jpg_array = np.frombuffer(r.content, dtype=np.uint8)
            frame = cv2.imdecode(jpg_array, cv2.IMREAD_COLOR)

            if frame is not None:
                print("Foto recibida y decodificada OK")
                yield frame
            else:
                print("Foto inválida o corrupta")

        except requests.exceptions.RequestException as e:
            print(f"Error al pedir foto: {e}")
        except Exception as e:
            print(f"Error inesperado: {e}")

        time.sleep(interval)  # Controla el FPS aproximado (0.5s → ~2 FPS)

# Iniciar el generador de frames
frame_generator = get_frames_from_capture(STREAM_URL, interval=0.002)

frame_count = 0
start_time = time.time()

print("¡Intentando recibir frames! Apunta la cámara al coral...\n")

# ===================== BUCLE PRINCIPAL =====================
"""
Bucle principal que obtiene frames del generador, los clasifica y muestra el resultado.
"""
while True:
    try:
        frame = next(frame_generator)
    except StopIteration:
        print("Generador terminado inesperadamente")
        break
    except Exception as e:
        print(f"Error al obtener frame: {e}")
        time.sleep(1)
        continue

    if frame is None:
        continue

    frame_count += 1

    # Redimensionamos para el modelo YOLO
    frame_resized = cv2.resize(frame, (400, 400))

    # Clasificación
    results = model(frame_resized, imgsz=400, conf=0.3, verbose=False)[0]
    probs = results.probs

    top1_idx = probs.top1
    top1_conf = probs.top1conf.item()
    top1_name = results.names[top1_idx]

    top5_idx = probs.top5
    top5_conf = probs.top5conf.tolist()
    top3_idx = top5_idx[:3]
    top3_conf = top5_conf[:3]

# ===================== OVERLAY =====================
    # Dibujar overlay con resultados de clasificación.
    overlay = frame.copy()

    # Rectángulo que ocupa ~1/3 de la ventana (ancho ~350-380 px, alto ~130 px)
    cv2.rectangle(overlay, (10, 10), (380, 140), (0, 0, 0), -1)
    cv2.addWeighted(overlay, 0.65, frame, 0.35, 0, frame)

    # Top 1 grande
    cv2.putText(frame, f"{top1_name}", (20, 45), cv2.FONT_HERSHEY_SIMPLEX, 1.0, (0, 255, 100), 3)
    cv2.putText(frame, f"{top1_conf:.1%}", (20, 75), cv2.FONT_HERSHEY_SIMPLEX, 0.9, (0, 255, 100), 2)

    # Top 3 pequeños debajo
    y = 105
    for i in range(3):
        name = results.names[top3_idx[i]]
        conf = top3_conf[i]
        color = (0, 255, 100) if i == 0 else (180, 180, 180)
        cv2.putText(frame, f"{i+1}. {name}: {conf:.1%}", (20, y), cv2.FONT_HERSHEY_SIMPLEX, 0.6, color, 1)
        y += 20  # separación reducida para caber en ~1/3

    # FPS en esquina superior derecha (discreto)
    if frame_count % 30 == 0:
        elapsed = time.time() - start_time
        fps = frame_count / elapsed if elapsed > 0 else 0
        cv2.putText(frame, f"FPS: {fps:.1f}", (frame.shape[1] - 120, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 200, 255), 2)

    cv2.imshow("Clasificador de Corales (ESP32) - Presiona Q", frame)
    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

cv2.destroyAllWindows()

print("¡Programa terminado!")
