import tkinter as tk
from tkinter import ttk
import serial
import numpy as np
import math
import time

from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from matplotlib.figure import Figure

# === CONFIGURAR SERIAL ===
puerto_serial = serial.Serial('/dev/cu.usbserial-0001', 115200, timeout=1)  # Cambia el puerto según tu caso

# === FUNCIONES DE FILTRO ===
def filtro_complementario(prev_angle, gyro_rate, accel_angle, dt, alpha=0.98):
    """Combina giroscopio y acelerómetro"""
    return alpha * (prev_angle + gyro_rate * dt) + (1 - alpha) * accel_angle

# === CLASE DE INTERFAZ ===
class App:
    def __init__(self, root):
        self.root = root
        self.root.title("Visualizador MPU6050 con Cubo 3D")
        self.root.geometry("800x700")
        self.root.configure(bg="#20232A")

        # === INTERFAZ ===
        ttk.Label(root, text="Cubo 3D controlado por MPU6050", font=("Arial", 16, "bold")).pack(pady=10)
        
        # === FIGURA 3D ===
        fig = Figure(figsize=(6, 6), dpi=100)
        self.ax = fig.add_subplot(111, projection="3d")
        self.ax.set_xlim([-1, 1])
        self.ax.set_ylim([-1, 1])
        self.ax.set_zlim([-1, 1])
        self.ax.set_facecolor("#FFFFFF")

        self.cubo, = self.ax.plot([], [], [], color="cyan", linewidth=2)
        self.canvas = FigureCanvasTkAgg(fig, master=root)
        self.canvas.get_tk_widget().pack(pady=20)

        # === VARIABLES ===
        self.roll = self.pitch = self.yaw = 0.0
        self.last_time = time.time()
        
        self.actualizar_datos()

    # === LEER SERIAL Y ACTUALIZAR ===
    def actualizar_datos(self):
        try:
            # ------- Para limpiar el buffer si se acumulan demasiados datos -------
            if puerto_serial.in_waiting > 200: # Si el umbral es mayor a 200 bytes -> varias lecturas 
                puerto_serial.reset_input_buffer()
                print("El buffer serial esta limpio para evitar saturación")

            raw_line = puerto_serial.readline()
            line = raw_line.decode('utf-8', errors='ignore').strip()

            if line:
                print(repr(line))
                try:
                    ax, ay, az, gx, gy, gz = map(float, line.split(','))
                    
                    # Escalado (ajustar según tu sensor)
                    gx /= 131.0
                    gy /= 131.0
                    gz /= 131.0
                    ax /= 16384.0
                    ay /= 16384.0
                    az /= 16384.0

                    # Calcular ángulos del acelerómetro
                    accel_roll = math.degrees(math.atan2(ay, az))
                    accel_pitch = math.degrees(math.atan2(-ax, math.sqrt(ay**2 + az**2)))

                    # Filtro complementario
                    now = time.time()
                    dt = now - self.last_time
                    self.last_time = now

                    self.roll = filtro_complementario(self.roll, gx, accel_roll, dt)
                    self.pitch = filtro_complementario(self.pitch, gy, accel_pitch, dt)
                    self.yaw += gz * dt  # Solo giroscopio para yaw

                    self.dibujar_cubo()

                except ValueError:
                    pass

        except serial.SerialException:
            print("Error de conexión con el puerto serial.")

        except Exception as ex:
            print(f"Error inesperado: {ex}")
        
        # Esta es la instrucción de frecuancia para actualizar los datos
        # La frecuencia es de 50 Hz que equivale a 20 ms
        self.root.after(20, self.actualizar_datos)

    # === DIBUJAR CUBO 3D ===
    def dibujar_cubo(self):
        # Definir vértices del cubo
        r = 0.5
        vertices = np.array([
            [-r, -r, -r],
            [r, -r, -r],
            [r, r, -r],
            [-r, r, -r],
            [-r, -r, r],
            [r, -r, r],
            [r, r, r],
            [-r, r, r]
        ])

        # Rotaciones (roll, pitch, yaw)
        roll_rad = math.radians(self.roll) 
        pitch_rad = math.radians(self.pitch) 
        yaw_rad = math.radians(self.yaw)

        Rx = np.array([
            [1, 0, 0],
            [0, math.cos(roll_rad), -math.sin(roll_rad)],
            [0, math.sin(roll_rad), math.cos(roll_rad)]
        ])
        Ry = np.array([
            [math.cos(pitch_rad), 0, math.sin(pitch_rad)],
            [0, 1, 0],
            [-math.sin(pitch_rad), 0, math.cos(pitch_rad)]
        ])
        Rz = np.array([
            [math.cos(yaw_rad), -math.sin(yaw_rad), 0],
            [math.sin(yaw_rad), math.cos(yaw_rad), 0],
            [0, 0, 1]
        ])

        R = Rz @ Ry @ Rx
        rotados = vertices @ R.T

        # Caras del cubo
        caras = [
            [0, 1, 2, 3],
            [4, 5, 6, 7],
            [0, 1, 5, 4],
            [2, 3, 7, 6],
            [1, 2, 6, 5],
            [0, 3, 7, 4]
        ]

        self.ax.cla()
        self.ax.set_xlim([-1, 1])
        self.ax.set_ylim([-1, 1])
        self.ax.set_zlim([-1, 1])
        self.ax.set_facecolor("#0090FF")

        for c in caras:
            x = rotados[c, 0]
            y = rotados[c, 1]
            z = rotados[c, 2]
            self.ax.plot_trisurf(x, y, z, color="cyan", alpha=0.6, edgecolor="white")

        self.canvas.draw()

# === INICIAR INTERFAZ ===
root = tk.Tk()
app = App(root)
root.mainloop()
