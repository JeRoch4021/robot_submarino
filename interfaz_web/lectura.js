// importar THREE.js y GLTFLoader para cargar y renderizar el modelo 3D
import * as THREE from 'three';
import { GLTFLoader } from 'three/addons/loaders/GLTFLoader.js';

// URL de la API que proporciona los datos de los sensores
const API_URL = 'https://4i25h1owl2.execute-api.us-east-2.amazonaws.com/v1/data';
const container = document.getElementById("gyro3d"); // Contenedor para el renderizado 3D

// Configuración básica de la escena 3D
const scene = new THREE.Scene();
scene.background = null;  // transparente para que combine con tu fondo

// Configuración de la cámara
const camera = new THREE.PerspectiveCamera(35, container.clientWidth / container.clientHeight, 0.1, 2000);
camera.position.set(0, 4, 15);
camera.lookAt(0, 0, 0);

// Configuración del renderizador
const renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true });
renderer.setSize(container.clientWidth, container.clientHeight);
renderer.setPixelRatio(window.devicePixelRatio);
renderer.outputColorSpace = THREE.SRGBColorSpace;
container.appendChild(renderer.domElement);

// Luces 
scene.add(new THREE.AmbientLight(0xffffff, 1.2));

const dirLight = new THREE.DirectionalLight(0xffffff, 2.5);
dirLight.position.set(5,2, 8);

// MODELO
const loader = new GLTFLoader();
let model;

/**
 * Cargar el modelo 3D del ajolote desde un archivo GLB.
 * Centrarlo, escalarlo y posicionarlo adecuadamente en la escena.
 */
loader.load('./ajolote.glb', (gltf) => {
  model = gltf.scene;

  // Centrar el modelo en el origen
  const box = new THREE.Box3().setFromObject(model);
  const center = box.getCenter(new THREE.Vector3());
  model.position.sub(center);

  // Escalar para que quepa bien (ajusta el 3.5 si es muy grande/pequeño)
  const size = box.getSize(new THREE.Vector3());
  const maxDim = Math.max(size.x, size.y, size.z);
  const scaleFactor = 6.0 / maxDim;
  model.scale.multiplyScalar(scaleFactor);

  // Posición fija (centrado, un poco arriba si quieres)
  model.position.set(0, 0, 0);  // y = 0.3 para que no toque el "suelo"

  model.rotation.order = 'YXZ';
  scene.add(model);

}, undefined, (error) => {
  console.error('Error cargando GLB:', error);
});

// ==== Filtro Complementario para orientación ====
const ACCEL_ESCALA = 16384.0;
const GYRO_ESCALA = 131.0;
const ALPHA = 0.98;

// Variables para almacenar la orientación actual
let currentPitch = 0.0;
let currentRoll = 0.0;
let currentYaw = 0.0;
let lastTime = Date.now();

/**
 * Función para calcular la orientación (roll, pitch, yaw) usando un filtro complementario.
 * @param {*} data - Datos del sensor MPU (acelerómetro y giroscopio)
 * @returns {Object} - Objeto con las propiedades roll, pitch y yaw en radianes
 */
function calcularOrientacionMPU(data) {
  const now = Date.now();
  const dt = (now - lastTime) / 1000.0;
  lastTime = now;
  
  const ax = data.AccelX / ACCEL_ESCALA;
  const ay = data.AccelY / ACCEL_ESCALA;
  const az = data.AccelZ / ACCEL_ESCALA;

  const gx = (data.GyroX / GYRO_ESCALA) * (Math.PI / 180);
  const gy = (data.GyroY / GYRO_ESCALA) * (Math.PI / 180);
  const gz = (data.GyroZ / GYRO_ESCALA) * (Math.PI / 180);

  const accRoll = Math.atan2(ay, az);
  const accPitch = Math.atan2(-ax, Math.sqrt(ay * ay + az * az));

  currentRoll = ALPHA * (currentRoll + gx * dt) + (1 - ALPHA) * accRoll;
  currentPitch = ALPHA * (currentPitch + gy * dt) + (1 - ALPHA) * accPitch;
  currentYaw += gz * dt;

  return {
    roll: currentRoll,
    pitch: currentPitch,
    yaw: currentYaw
  };
}

// ==== Actualización desde API ====
/**
 * Función para obtener datos de la API y actualizar la interfaz y el modelo 3D.
 * También actualiza los valores en el HTML.
 */
async function updateFromAPI() {
  try {
    const response = await fetch(API_URL);
    if (!response.ok) throw new Error('Error API');

    // Recibir los objetos de la respuesta de Lambda/API Gateway
    const resultados = await response.json();

    // Parsear el body porque viene como un string y tomamos el primer elemento [0]
    // Usamos JSON.parse porque el body viene como un string escapado
    const itmes = resultados[0]
    const data = itmes.Data;

    // Actualizar valores en el HTML de acuerdo a los id correspondientes
    document.getElementById('accel-x').textContent = (data.AccelX / 16384)?.toFixed(2) ?? '-';
    document.getElementById('accel-y').textContent = (data.AccelY / 16384)?.toFixed(2) ?? '-';
    document.getElementById('accel-z').textContent = (data.AccelZ / 16384)?.toFixed(2) ?? '-';
    document.getElementById('gyro-x').textContent  = (data.GyroX / 131)?.toFixed(2)  ?? '-';
    document.getElementById('gyro-y').textContent  = (data.GyroY / 131)?.toFixed(2)  ?? '-';
    document.getElementById('gyro-z').textContent  = (data.GyroZ / 131)?.toFixed(2)  ?? '-';
    document.getElementById('temp-value').textContent  = data.Temperatura?.toFixed(1)  ?? '-';
    document.getElementById('humidity-value').textContent  = data.Humedad?.toFixed(1)  ?? '-';
    document.getElementById('salinidad').textContent  = data.salinidad?.toFixed(1)  ?? '-';

    // Rotación para el modelo
    if (model) {
      // Ajustar la orientación del modelo 3D usando el filtro complementario
      const orientacion = calcularOrientacionMPU(data);

      model.rotation.order = 'YXZ';

      model.rotation.z = -orientacion.roll;
      model.rotation.x = -orientacion.pitch;
      model.rotation.y = orientacion.yaw;
    }

    // Error al procesar datos
  } catch (err) {
    console.error('Error al procesar datos:', err);
  }
}

// Actualización periódica de la API y el modelo 3D
updateFromAPI();
setInterval(updateFromAPI, 50);

/**
 * Render loop.
 * Función de animación para renderizar la escena 3D.
 * Se llama a sí misma usando requestAnimationFrame para crear un bucle de renderizado.
 */
function animate() {
  requestAnimationFrame(animate);
  renderer.render(scene, camera);
}
animate();

/**
 * Evento para manejar el cambio de tamaño de la ventana.
 * Actualiza la cámara y el renderizador para ajustarse al nuevo tamaño del contenedor.
 */
window.addEventListener('resize', () => {
  const w = container.clientWidth;
  const h = container.clientHeight;
  camera.aspect = w / h;
  camera.updateProjectionMatrix();
  renderer.setSize(w, h);
});