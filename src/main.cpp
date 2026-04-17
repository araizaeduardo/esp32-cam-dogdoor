#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>
#include <time.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <FS.h>
#include <SPIFFS.h>
#include <WiFiManager.h>

// ============================================
// CONFIGURACIÓN DE LA CÁMARA (AI THINKER ESP32-CAM)
// ============================================
#define CAMERA_MODEL_AI_THINKER

#if defined(CAMERA_MODEL_AI_THINKER)
  #define PWDN_GPIO_NUM     32
  #define RESET_GPIO_NUM    -1
  #define XCLK_GPIO_NUM      0
  #define SIOD_GPIO_NUM     26
  #define SIOC_GPIO_NUM     27
  
  #define Y9_GPIO_NUM       35
  #define Y8_GPIO_NUM       34
  #define Y7_GPIO_NUM       39
  #define Y6_GPIO_NUM       36
  #define Y5_GPIO_NUM       21
  #define Y4_GPIO_NUM       19
  #define Y3_GPIO_NUM       18
  #define Y2_GPIO_NUM        5
  #define VSYNC_GPIO_NUM    25
  #define HREF_GPIO_NUM     23
  #define PCLK_GPIO_NUM     22
#endif

// ============================================
// CONFIGURACIÓN DE RED WIFI
// ============================================
char wifiSSID[32] = "";  // Configurar desde WiFiManager o web
char wifiPassword[64] = "";  // Configurar desde WiFiManager o web

// ============================================
// CONFIGURACIÓN DE PINES GPIO
// ============================================
#define RELAY_PIN           12    // GPIO para el relay
#define IR_SENSOR_PIN       13    // GPIO para el sensor infrarrojo
#define ISD1820_PLAY_PIN    14    // GPIO para el módulo ISD1820 (play)
#define FLASH_LED_PIN       4     // GPIO para el LED flash

// ============================================
// CONFIGURACIÓN DE HORARIOS PARA AUDIO
// ============================================
// Horas en formato 24h (ej: 8 para las 8:00 AM, 20 para las 8:00 PM)
int AUDIO_HOURS[] = {8, 12, 18, 21};  // Horas para reproducir audio
const int NUM_AUDIO_HOURS = 4;

// ============================================
// CONFIGURACIÓN DE FLASH LED
// ============================================
int flashStartHour = 18;  // Hora inicio (18:00)
int flashEndHour = 7;     // Hora fin (7:00) - cruza medianoche
bool flashAutoMode = false;  // Activación automática por movimiento
bool flashManualState = true;  // Estado manual del flash

// ============================================
// CONFIGURACIÓN DE ZONA HORARIA
// ============================================
int timezoneOffset = -28800;  // Offset en segundos (UTC-8 = Los Ángeles PST)

// ============================================
// ESTADOS DEL SISTEMA
// ============================================
enum SystemState {
  WAITING_FOR_DOG,        // Esperando que el perro se acerque
  DOG_DETECTED,           // Perro detectado por cámara, relay activado
  WAITING_FOR_EXIT,       // Esperando que el sensor IR detecte salida
  DOG_OUTSIDE,            // Perro fuera, relay activo
  WAITING_FOR_RETURN,     // Esperando que el sensor IR detecte retorno
  DOG_RETURNED            // Perro regresó, relay desactivado
};

SystemState currentState = WAITING_FOR_DOG;

// ============================================
// VARIABLES GLOBALES
// ============================================
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", timezoneOffset, 60000);  // Offset configurable, actualiza cada 60s

camera_fb_t *previousFrame = NULL;
unsigned long lastFrameTime = 0;
const unsigned long FRAME_INTERVAL = 500;  // 500ms entre frames para detección

unsigned long lastAudioTime = 0;
const unsigned long AUDIO_COOLDOWN = 300000;  // 5 minutos mínimo entre audios

bool relayState = false;
bool manualMode = false;  // Modo manual para control del relay


// ============================================
// FUNCIONES DE CONTROL DEL FLASH
// ============================================
bool isFlashScheduleActive() {
  timeClient.update();
  int currentHour = timeClient.getHours();
  
  // Verificar si está en el rango de horario (considerando cruce de medianoche)
  if (flashStartHour <= flashEndHour) {
    // Rango normal (ej: 8:00 - 18:00)
    return (currentHour >= flashStartHour && currentHour < flashEndHour);
  } else {
    // Cruce de medianoche (ej: 18:00 - 7:00)
    return (currentHour >= flashStartHour || currentHour < flashEndHour);
  }
}

void setFlash(bool state) {
  digitalWrite(FLASH_LED_PIN, state ? HIGH : LOW);
  flashManualState = state;
}

// ============================================
// FUNCIONES DE PERSISTENCIA (SPIFFS)
// ============================================
void factoryReset() {
  SPIFFS.remove("/config.txt");
  Serial.println("Factory reset realizado - configuración eliminada");
}

void saveConfig() {
  File file = SPIFFS.open("/config.txt", "w");
  if (file) {
    file.print("wifi_ssid=");
    file.println(wifiSSID);
    file.print("wifi_pass=");
    file.println(wifiPassword);
    file.print("timezone=");
    file.println(timezoneOffset);
    file.print("flash_start=");
    file.println(flashStartHour);
    file.print("flash_end=");
    file.println(flashEndHour);
    file.close();
    Serial.println("Configuración guardada");
  } else {
    Serial.println("Error al guardar configuración");
  }
}

void loadConfig() {
  if (SPIFFS.exists("/config.txt")) {
    File file = SPIFFS.open("/config.txt", "r");
    if (file) {
      while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        
        if (line.startsWith("wifi_ssid=")) {
          String ssid = line.substring(10);
          ssid.toCharArray(wifiSSID, 32);
          Serial.print("SSID cargado: ");
          Serial.println(wifiSSID);
        } else if (line.startsWith("wifi_pass=")) {
          String pass = line.substring(10);
          pass.toCharArray(wifiPassword, 64);
          Serial.println("Password cargado");
        } else if (line.startsWith("timezone=")) {
          timezoneOffset = line.substring(9).toInt();
          Serial.print("Zona horaria cargada: ");
          Serial.println(timezoneOffset / 3600);
        } else if (line.startsWith("flash_start=")) {
          flashStartHour = line.substring(12).toInt();
          Serial.print("Horario flash inicio cargado: ");
          Serial.println(flashStartHour);
        } else if (line.startsWith("flash_end=")) {
          flashEndHour = line.substring(10).toInt();
          Serial.print("Horario flash fin cargado: ");
          Serial.println(flashEndHour);
        }
      }
      file.close();
    }
  } else {
    Serial.println("No existe archivo de configuración, usando valores por defecto");
  }
}

// ============================================
// CONFIGURACIÓN DEL SERVIDOR WEB
// ============================================
WebServer server(80);

// ============================================
// FUNCIONES DE DETECCIÓN DE MOVIMIENTO
// ============================================
bool detectMotion() {
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) return false;

  bool motionDetected = false;
  
  if (previousFrame != NULL && fb->len == previousFrame->len) {
    int diffCount = 0;
    int threshold = 30;  // Umbral de diferencia de píxeles
    int maxDiff = fb->len / 100;  // Máximo 1% de píxeles diferentes
    
    for (int i = 0; i < fb->len && diffCount < maxDiff; i++) {
      if (abs(fb->buf[i] - previousFrame->buf[i]) > threshold) {
        diffCount++;
      }
    }
    
    if (diffCount > maxDiff / 2) {
      motionDetected = true;
    }
  }

  if (previousFrame != NULL) {
    esp_camera_fb_return(previousFrame);
  }
  previousFrame = fb;

  return motionDetected;
}

// ============================================
// FUNCIONES DE CONTROL DE RELAY
// ============================================
void setRelay(bool state) {
  digitalWrite(RELAY_PIN, state ? HIGH : LOW);
  relayState = state;
  Serial.printf("Relay %s\n", state ? "ACTIVADO" : "DESACTIVADO");
}

// ============================================
// FUNCIONES DE CONTROL DE SENSOR IR
// ============================================
bool readIRSensor() {
  // Asume sensor IR activo bajo (LOW cuando detecta)
  return digitalRead(IR_SENSOR_PIN) == LOW;
}

// ============================================
// FUNCIONES DE CONTROL DE ISD1820
// ============================================
void playAudio() {
  Serial.println("Reproduciendo audio...");
  digitalWrite(ISD1820_PLAY_PIN, HIGH);
  delay(200);  // Pulso de 200ms para activar reproducción
  digitalWrite(ISD1820_PLAY_PIN, LOW);
}

// ============================================
// FUNCIONES DE TIEMPO
// ============================================
bool isAudioHour() {
  timeClient.update();
  int currentHour = timeClient.getHours();
  
  for (int i = 0; i < NUM_AUDIO_HOURS; i++) {
    if (currentHour == AUDIO_HOURS[i]) {
      return true;
    }
  }
  return false;
}

bool shouldPlayAudio() {
  if (!isAudioHour()) return false;
  if (millis() - lastAudioTime < AUDIO_COOLDOWN) return false;
  return true;
}

// ============================================
// MÁQUINA DE ESTADOS
// ============================================
void updateStateMachine() {
  static unsigned long stateChangeTime = 0;
  const unsigned long DEBOUNCE_TIME = 2000;  // 2 segundos debounce
  
  switch (currentState) {
    case WAITING_FOR_DOG:
      // En modo manual, no detectar automáticamente
      if (!manualMode) {
        if (millis() - lastFrameTime > FRAME_INTERVAL) {
          lastFrameTime = millis();
          if (detectMotion()) {
            Serial.println("¡Perro detectado por cámara!");
            setRelay(true);
            
            // Activar flash si está en modo automático y dentro del horario
            if (flashAutoMode && isFlashScheduleActive()) {
              setFlash(true);
              Serial.println("Flash activado por detección");
            }
            
            currentState = DOG_DETECTED;
            stateChangeTime = millis();
          }
        }
        
        // Reproducir audio si es hora apropiada
        if (shouldPlayAudio()) {
          playAudio();
          lastAudioTime = millis();
        }
      }
      break;

    case DOG_DETECTED:
      if (millis() - stateChangeTime > DEBOUNCE_TIME) {
        Serial.println("Esperando que el perro salga (sensor IR)...");
        currentState = WAITING_FOR_EXIT;
        stateChangeTime = millis();
      }
      break;

    case WAITING_FOR_EXIT:
      if (readIRSensor()) {
        Serial.println("Sensor IR detectó salida del perro");
        currentState = DOG_OUTSIDE;
        stateChangeTime = millis();
      }
      // Timeout por si el perro no sale
      if (millis() - stateChangeTime > 30000) {  // 30 segundos
        Serial.println("Timeout: perro no salió, regresando a espera");
        setRelay(false);
        currentState = WAITING_FOR_DOG;
      }
      break;

    case DOG_OUTSIDE:
      Serial.println("Perro fuera - relay activo esperando retorno");
      currentState = WAITING_FOR_RETURN;
      stateChangeTime = millis();
      break;

    case WAITING_FOR_RETURN:
      if (readIRSensor()) {
        Serial.println("Sensor IR detectó retorno del perro");
        // Verificar con cámara que sea el perro
        if (millis() - lastFrameTime > FRAME_INTERVAL) {
          lastFrameTime = millis();
          if (detectMotion()) {
            Serial.println("¡Perro confirmado por cámara!");
            setRelay(false);
            currentState = DOG_RETURNED;
            stateChangeTime = millis();
          }
        }
      }
      break;

    case DOG_RETURNED:
      if (millis() - stateChangeTime > DEBOUNCE_TIME) {
        Serial.println("Ciclo completado - esperando nuevo ciclo");
        currentState = WAITING_FOR_DOG;
        stateChangeTime = millis();
      }
      break;
  }
}

// ============================================
// HANDLERS DEL SERVIDOR WEB
// ============================================
String getStateName() {
  switch (currentState) {
    case WAITING_FOR_DOG: return "Esperando perro";
    case DOG_DETECTED: return "Perro detectado";
    case WAITING_FOR_EXIT: return "Esperando salida";
    case DOG_OUTSIDE: return "Perro fuera";
    case WAITING_FOR_RETURN: return "Esperando retorno";
    case DOG_RETURNED: return "Perro regresó";
    default: return "Desconocido";
  }
}

void handleRoot() {
  String html = "<!DOCTYPE html><html lang='es'><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>ESP32-CAM - Sistema Perro</title>";
  html += "<style>";
  html += "* { margin: 0; padding: 0; box-sizing: border-box; }";
  html += "body { font-family: Arial, sans-serif; background: #1a1a2e; color: #eee; padding: 20px; }";
  html += ".container { max-width: 1200px; margin: 0 auto; }";
  html += "h1 { text-align: center; color: #e94560; margin-bottom: 30px; }";
  html += ".grid { display: grid; grid-template-columns: 1fr 1fr; gap: 20px; }";
  html += ".card { background: #16213e; padding: 20px; border-radius: 10px; box-shadow: 0 4px 6px rgba(0,0,0,0.3); }";
  html += ".card h2 { color: #0f3460; margin-bottom: 15px; font-size: 1.3em; }";
  html += ".status-item { display: flex; justify-content: space-between; padding: 10px 0; border-bottom: 1px solid #0f3460; }";
  html += ".status-label { color: #a0a0a0; }";
  html += ".status-value { font-weight: bold; }";
  html += ".status-value.active { color: #00ff88; }";
  html += ".status-value.inactive { color: #ff4444; }";
  html += ".btn { padding: 10px 20px; border: none; border-radius: 5px; cursor: pointer; font-size: 14px; margin: 5px; }";
  html += ".btn-primary { background: #e94560; color: white; }";
  html += ".btn-success { background: #00ff88; color: #1a1a2e; }";
  html += ".btn-danger { background: #ff4444; color: white; }";
  html += ".btn:hover { opacity: 0.9; }";
  html += ".camera-container { text-align: center; }";
  html += "#camera-feed { max-width: 100%; border-radius: 10px; }";
  html += ".schedule-input { width: 60px; padding: 5px; margin: 5px; }";
  html += "input[type='number'] { background: #0f3460; border: 1px solid #e94560; color: #eee; padding: 8px; border-radius: 5px; }";
  html += ".mode-toggle { display: flex; gap: 10px; margin: 15px 0; }";
  html += ".mode-toggle label { cursor: pointer; }";
  html += "@media (max-width: 768px) { .grid { grid-template-columns: 1fr; } }";
  html += "</style></head><body>";
  html += "<div class='container'>";
  html += "<h1>🐕 Sistema de Detección de Perro</h1>";
  html += "<div class='grid'>";
  html += "<div class='card'>";
  html += "<h2>📊 Estado del Sistema</h2>";
  html += "<div class='status-item'>";
  html += "<span class='status-label'>Estado:</span>";
  html += "<span class='status-value' id='state'>" + getStateName() + "</span>";
  html += "</div>";
  html += "<div class='status-item'>";
  html += "<span class='status-label'>Relay:</span>";
  html += "<span class='status-value " + String(relayState ? "active" : "inactive") + "' id='relay'>" + String(relayState ? "ACTIVO" : "INACTIVO") + "</span>";
  html += "</div>";
  html += "<div class='status-item'>";
  html += "<span class='status-label'>Sensor IR:</span>";
  html += "<span class='status-value " + String(readIRSensor() ? "active" : "inactive") + "' id='ir'>" + String(readIRSensor() ? "ACTIVO" : "INACTIVO") + "</span>";
  html += "</div>";
  html += "<div class='status-item'>";
  html += "<span class='status-label'>Modo:</span>";
  html += "<span class='status-value' id='mode'>" + String(manualMode ? "MANUAL" : "AUTOMÁTICO") + "</span>";
  html += "</div>";
  html += "<div class='status-item'>";
  html += "<span class='status-label'>Hora actual:</span>";
  html += "<span class='status-value' id='time'>--:--:--</span>";
  html += "</div></div>";
  
  html += "<div class='card'>";
  html += "<h2>🎮 Control Manual</h2>";
  html += "<div class='mode-toggle'>";
  html += "<label><input type='radio' name='mode' value='auto' " + String(!manualMode ? "checked" : "") + " onchange='setMode(false)'> Automático</label>";
  html += "<label><input type='radio' name='mode' value='manual' " + String(manualMode ? "checked" : "") + " onchange='setMode(true)'> Manual</label>";
  html += "</div>";
  html += "<button class='btn btn-success' onclick='relayOn()'>Activar Relay</button>";
  html += "<button class='btn btn-danger' onclick='relayOff()'>Desactivar Relay</button>";
  html += "<button class='btn btn-primary' onclick='playAudio()'>Reproducir Audio</button>";
  html += "<button class='btn btn-primary' onclick='resetSystem()'>Reiniciar Sistema</button>";
  html += "</div>";
  
  html += "<div class='card'>";
  html += "<h2>� Control de Flash</h2>";
  html += "<div class='mode-toggle'>";
  html += "<label><input type='radio' name='flashmode' value='auto' " + String(flashAutoMode ? "checked" : "") + " onchange='setFlashMode(true)'> Automático</label>";
  html += "<label><input type='radio' name='flashmode' value='manual' " + String(!flashAutoMode ? "checked" : "") + " onchange='setFlashMode(false)'> Manual</label>";
  html += "</div>";
  html += "<button class='btn btn-success' onclick='flashOn()'>Activar Flash</button>";
  html += "<button class='btn btn-danger' onclick='flashOff()'>Desactivar Flash</button>";
  html += "<p>Horario de activación: " + String(flashStartHour) + ":00 - " + String(flashEndHour) + ":00</p>";
  html += "<div>";
  html += "<label>Inicio (0-23): <input type='number' min='0' max='23' value='" + String(flashStartHour) + "' id='flashStart'></label>";
  html += "<label>Fin (0-23): <input type='number' min='0' max='23' value='" + String(flashEndHour) + "' id='flashEnd'></label>";
  html += "</div>";
  html += "<button class='btn btn-primary' onclick='saveFlashSchedule()'>Guardar Horario</button>";
  html += "</div>";
  
  html += "<div class='card'>";
  html += "<h2>📅 Configuración de Horarios</h2>";
  html += "<p>Horas para reproducir audio (formato 24h):</p>";
  html += "<div id='schedule-inputs'>";
  html += "<input type='number' class='schedule-input' min='0' max='23' value='" + String(AUDIO_HOURS[0]) + "'>";
  html += "<input type='number' class='schedule-input' min='0' max='23' value='" + String(AUDIO_HOURS[1]) + "'>";
  html += "<input type='number' class='schedule-input' min='0' max='23' value='" + String(AUDIO_HOURS[2]) + "'>";
  html += "<input type='number' class='schedule-input' min='0' max='23' value='" + String(AUDIO_HOURS[3]) + "'>";
  html += "</div>";
  html += "<button class='btn btn-primary' onclick='saveSchedule()'>Guardar Horarios</button>";
  html += "</div>";
  
  html += "<div class='card'>";
  html += "<h2>🌍 Configuración de Zona Horaria</h2>";
  html += "<p>Offset UTC (en horas, ej: -5 para UTC-5, +1 para UTC+1):</p>";
  html += "<input type='number' min='-12' max='14' step='1' value='" + String(timezoneOffset / 3600) + "' id='timezoneOffset'>";
  html += "<button class='btn btn-primary' onclick='saveTimezone()'>Guardar Zona Horaria</button>";
  html += "</div>";
  
  html += "<div class='card'>";
  html += "<h2>📶 Configuración WiFi</h2>";
  html += "<p>SSID:</p>";
  html += "<input type='text' id='wifiSSID' value='" + String(wifiSSID) + "' style='width: 100%; padding: 8px; margin: 5px 0; background: #0f3460; border: 1px solid #e94560; color: #eee; border-radius: 5px;'>";
  html += "<p>Password:</p>";
  html += "<input type='password' id='wifiPassword' value='" + String(wifiPassword) + "' style='width: 100%; padding: 8px; margin: 5px 0; background: #0f3460; border: 1px solid #e94560; color: #eee; border-radius: 5px;'>";
  html += "<button class='btn btn-primary' onclick='saveWiFi()'>Guardar WiFi</button>";
  html += "</div>";
  
  html += "<div class='card camera-container'>";
  html += "<h2>📷 Vista de Cámara</h2>";
  html += "<img id='camera-feed' src='/capture' alt='Cámara' onload='setTimeout(() => this.src=\"/capture?t=\" + new Date().getTime(), 500)'>";
  html += "<p><small>Actualización automática cada 500ms</small></p>";
  html += "</div></div></div>";
  
  html += "<script>";
  html += "function updateStatus() {";
  html += "fetch('/status').then(r => r.json()).then(data => {";
  html += "document.getElementById('relay').textContent = data.relay ? 'ACTIVO' : 'INACTIVO';";
  html += "document.getElementById('relay').className = 'status-value ' + (data.relay ? 'active' : 'inactive');";
  html += "document.getElementById('ir').textContent = data.ir_sensor ? 'ACTIVO' : 'INACTIVO';";
  html += "document.getElementById('ir').className = 'status-value ' + (data.ir_sensor ? 'active' : 'inactive');";
  html += "});}";
  html += "function updateTime() {";
  html += "fetch('/time').then(r => r.text()).then(time => {";
  html += "document.getElementById('time').textContent = time;";
  html += "});}";
  html += "function setMode(manual) {";
  html += "fetch('/mode?manual=' + manual).then(r => r.text()).then(() => {";
  html += "document.getElementById('mode').textContent = manual ? 'MANUAL' : 'AUTOMÁTICO';";
  html += "updateStatus();";
  html += "});}";
  html += "function relayOn() { fetch('/relay?state=on').then(() => updateStatus()); }";
  html += "function relayOff() { fetch('/relay?state=off').then(() => updateStatus()); }";
  html += "function playAudio() { fetch('/audio').then(() => alert('Audio reproduciendo...')); }";
  html += "function resetSystem() { if (confirm('¿Reiniciar el sistema?')) { fetch('/reset').then(() => location.reload()); } }";
  html += "function flashOn() { fetch('/flash?state=on').then(() => updateStatus()); }";
  html += "function flashOff() { fetch('/flash?state=off').then(() => updateStatus()); }";
  html += "function setFlashMode(auto) { fetch('/flashmode?auto=' + auto).then(() => location.reload()); }";
  html += "function saveFlashSchedule() {";
  html += "const start = document.getElementById('flashStart').value;";
  html += "const end = document.getElementById('flashEnd').value;";
  html += "fetch('/flashschedule', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ start: start, end: end }) }).then(() => alert('Horario de flash guardado'));";
  html += "}";
  html += "function saveSchedule() {";
  html += "const inputs = document.querySelectorAll('.schedule-input');";
  html += "const hours = Array.from(inputs).map(i => i.value);";
  html += "fetch('/schedule', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ hours: hours }) }).then(() => alert('Horarios guardados'));";
  html += "}";
  html += "function saveTimezone() {";
  html += "const offset = document.getElementById('timezoneOffset').value;";
  html += "fetch('/timezone', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ offset: offset }) }).then(() => alert('Zona horaria guardada. Reinicia el dispositivo.'));";
  html += "}";
  html += "function saveWiFi() {";
  html += "const ssid = document.getElementById('wifiSSID').value;";
  html += "const pass = document.getElementById('wifiPassword').value;";
  html += "fetch('/wifi', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ ssid: ssid, password: pass }) }).then(() => alert('WiFi guardado. Reinicia el dispositivo.'));";
  html += "}";
  html += "setInterval(updateStatus, 1000);";
  html += "setInterval(updateTime, 1000);";
  html += "updateStatus(); updateTime();";
  html += "</script></body></html>";
  
  server.send(200, "text/html", html);
}

void handleCapture() {
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    server.send(500, "text/plain", "Error al capturar imagen");
    return;
  }

  WiFiClient client = server.client();
  String header = "HTTP/1.1 200 OK\r\n";
  header += "Content-Type: image/jpeg\r\n";
  header += "Content-Length: " + String(fb->len) + "\r\n";
  header += "Cache-Control: no-cache\r\n";
  header += "Connection: close\r\n\r\n";
  client.print(header);
  client.write(fb->buf, fb->len);

  esp_camera_fb_return(fb);
}

void handleStatus() {
  String json = "{";
  json += "\"state\":" + String(currentState) + ",";
  json += "\"relay\":" + String(relayState ? "true" : "false") + ",";
  json += "\"ir_sensor\":" + String(readIRSensor() ? "true" : "false") + ",";
  json += "\"manual\":" + String(manualMode ? "true" : "false") + ",";
  json += "\"flash\":" + String(flashManualState ? "true" : "false") + ",";
  json += "\"flash_auto\":" + String(flashAutoMode ? "true" : "false");
  json += "}";
  server.send(200, "application/json", json);
}

void handleTime() {
  timeClient.update();
  String timeStr = String(timeClient.getHours()) + ":" + 
                   String(timeClient.getMinutes()) + ":" + 
                   String(timeClient.getSeconds());
  server.send(200, "text/plain", timeStr);
}

void handleRelay() {
  if (server.hasArg("state")) {
    String state = server.arg("state");
    if (state == "on") {
      setRelay(true);
      server.send(200, "text/plain", "Relay activado");
    } else if (state == "off") {
      setRelay(false);
      server.send(200, "text/plain", "Relay desactivado");
    }
  }
}

void handleMode() {
  if (server.hasArg("manual")) {
    manualMode = server.arg("manual") == "true";
    server.send(200, "text/plain", manualMode ? "Modo manual" : "Modo automático");
  }
}

void handleAudio() {
  playAudio();
  server.send(200, "text/plain", "Audio iniciado");
}

void handleReset() {
  currentState = WAITING_FOR_DOG;
  setRelay(false);
  manualMode = false;
  server.send(200, "text/plain", "Sistema reiniciado");
}

void handleSchedule() {
  if (server.method() == HTTP_POST) {
    String body = server.arg("plain");
    // Parse JSON simple: {"hours":[8,12,18,21]}
    int startIdx = body.indexOf("[");
    int endIdx = body.indexOf("]");
    if (startIdx > 0 && endIdx > startIdx) {
      String hoursStr = body.substring(startIdx + 1, endIdx);
      int idx = 0;
      int commaPos = 0;
      while (commaPos >= 0 && idx < NUM_AUDIO_HOURS) {
        int nextComma = hoursStr.indexOf(",", commaPos);
        if (nextComma == -1) nextComma = hoursStr.length();
        String hourStr = hoursStr.substring(commaPos, nextComma);
        hourStr.trim();
        if (hourStr.length() > 0) {
          AUDIO_HOURS[idx] = hourStr.toInt();
          idx++;
        }
        commaPos = (nextComma < hoursStr.length()) ? nextComma + 1 : -1;
      }
      Serial.print("Horarios actualizados: ");
      for (int i = 0; i < NUM_AUDIO_HOURS; i++) {
        Serial.print(AUDIO_HOURS[i]);
        Serial.print(" ");
      }
      Serial.println();
      server.send(200, "text/plain", "Horarios actualizados correctamente");
    } else {
      server.send(400, "text/plain", "Formato JSON inválido");
    }
  } else {
    server.send(405, "text/plain", "Método no permitido");
  }
}

void handleFlash() {
  if (server.hasArg("state")) {
    String state = server.arg("state");
    if (state == "on") {
      setFlash(true);
      server.send(200, "text/plain", "Flash activado");
    } else if (state == "off") {
      setFlash(false);
      server.send(200, "text/plain", "Flash desactivado");
    }
  }
}

void handleFlashMode() {
  if (server.hasArg("auto")) {
    flashAutoMode = server.arg("auto") == "true";
    server.send(200, "text/plain", flashAutoMode ? "Modo automático" : "Modo manual");
  }
}

void handleFlashSchedule() {
  if (server.method() == HTTP_POST) {
    String body = server.arg("plain");
    // Parse JSON simple: {"start":18,"end":7}
    int startIdx = body.indexOf("\"start\":");
    int endIdx = body.indexOf("\"end\":");
    if (startIdx >= 0 && endIdx >= 0) {
      int startValIdx = startIdx + 8;
      int endValIdx = endIdx + 6;
      String startStr = body.substring(startValIdx);
      String endStr = body.substring(endValIdx);
      startStr.replace("\"", "");
      endStr.replace("\"", "");
      startStr.trim();
      endStr.trim();
      
      // Extraer números
      int startNum = startStr.toInt();
      int endNum = endStr.toInt();
      
      if (startNum >= 0 && startNum <= 23 && endNum >= 0 && endNum <= 23) {
        flashStartHour = startNum;
        flashEndHour = endNum;
        saveConfig();  // Guardar configuración
        Serial.print("Horario de flash actualizado: ");
        Serial.print(flashStartHour);
        Serial.print(":00 - ");
        Serial.print(flashEndHour);
        Serial.println(":00");
        server.send(200, "text/plain", "Horario de flash actualizado");
      } else {
        server.send(400, "text/plain", "Horas inválidas (0-23)");
      }
    } else {
      server.send(400, "text/plain", "Formato JSON inválido");
    }
  } else {
    server.send(405, "text/plain", "Método no permitido");
  }
}

void handleTimezone() {
  if (server.method() == HTTP_POST) {
    String body = server.arg("plain");
    // Parse JSON simple: {"offset":-5}
    int offsetIdx = body.indexOf("\"offset\":");
    if (offsetIdx >= 0) {
      int offsetValIdx = offsetIdx + 9;
      String offsetStr = body.substring(offsetValIdx);
      offsetStr.replace("\"", "");  // Eliminar comillas
      offsetStr.trim();
      
      // Extraer número
      int offsetHours = offsetStr.toInt();
      
      if (offsetHours >= -12 && offsetHours <= 14) {
        timezoneOffset = offsetHours * 3600;  // Convertir horas a segundos
        timeClient.setTimeOffset(timezoneOffset);
        saveConfig();  // Guardar configuración
        Serial.print("Zona horaria actualizada: UTC");
        if (offsetHours >= 0) {
          Serial.print("+");
        }
        Serial.println(offsetHours);
        server.send(200, "text/plain", "Zona horaria actualizada");
      } else {
        server.send(400, "text/plain", "Offset inválido (-12 a +14)");
      }
    } else {
      server.send(400, "text/plain", "Formato JSON inválido");
    }
  } else {
    server.send(405, "text/plain", "Método no permitido");
  }
}

void handleWiFi() {
  if (server.method() == HTTP_POST) {
    String body = server.arg("plain");
    // Parse JSON simple: {"ssid":"nombre","password":"pass"}
    int ssidIdx = body.indexOf("\"ssid\":");
    int passIdx = body.indexOf("\"password\":");
    if (ssidIdx >= 0 && passIdx >= 0) {
      int ssidValIdx = ssidIdx + 7;
      int passValIdx = passIdx + 11;
      
      String ssidStr = body.substring(ssidValIdx);
      String passStr = body.substring(passValIdx);
      
      // Extraer valores (hasta la comilla o coma)
      int ssidEnd = ssidStr.indexOf("\"");
      int passEnd = passStr.indexOf("\"");
      if (ssidEnd > 0) ssidStr = ssidStr.substring(0, ssidEnd);
      if (passEnd > 0) passStr = passStr.substring(0, passEnd);
      
      ssidStr.trim();
      passStr.trim();
      
      if (ssidStr.length() > 0 && ssidStr.length() < 32) {
        strncpy(wifiSSID, ssidStr.c_str(), 32);
        strncpy(wifiPassword, passStr.c_str(), 64);
        saveConfig();  // Guardar configuración
        
        Serial.print("WiFi configurado: ");
        Serial.println(wifiSSID);
        
        // Reiniciar para aplicar cambios
        server.send(200, "text/plain", "WiFi configurado. Reiniciando...");
        delay(1000);
        ESP.restart();
      } else {
        server.send(400, "text/plain", "SSID inválido");
      }
    } else {
      server.send(400, "text/plain", "Formato JSON inválido");
    }
  } else {
    server.send(405, "text/plain", "Método no permitido");
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();
  Serial.println("=== SISTEMA DE DETECCIÓN DE PERRO ===");

  // ============================================
  // DETECCIÓN DE FACTORY RESET (BOTÓN BOOT + RESET)
  // ============================================
  // Si el botón BOOT (GPIO0) está presionado durante el arranque,
  // esperar 3 segundos. Si se mantiene presionado, hacer factory reset.
  pinMode(0, INPUT_PULLUP);
  
  if (digitalRead(0) == LOW) {
    Serial.println("Botón BOOT detectado. Mantén presionado 3s para factory reset...");
    unsigned long pressStart = millis();
    bool factoryResetTriggered = false;
    
    while (digitalRead(0) == LOW) {
      if (millis() - pressStart >= 3000) {
        factoryResetTriggered = true;
        break;
      }
      delay(100);
      Serial.print(".");
    }
    Serial.println();
    
    if (factoryResetTriggered) {
      Serial.println("¡Factory reset activado!");
      
      // Borrar credenciales WiFi del WiFiManager
      WiFiManager wm;
      wm.resetSettings();
      Serial.println("Credenciales WiFi borradas");
      
      // Eliminar configuración de SPIFFS
      if (SPIFFS.begin(true)) {
        SPIFFS.remove("/config.txt");
        Serial.println("Configuración SPIFFS eliminada");
        SPIFFS.end();
      }
      
      Serial.println("Factory reset completo. Reiniciando en 2s...");
      delay(2000);
      ESP.restart();
    } else {
      Serial.println("Botón liberado antes de 3s. Continuando normal...");
    }
  }

  // ============================================
  // INICIALIZAR SPIFFS
  // ============================================
  if (!SPIFFS.begin(true)) {
    Serial.println("Error al montar SPIFFS");
  } else {
    Serial.println("SPIFFS montado correctamente");
    loadConfig();  // Cargar configuración guardada
  }

  // ============================================
  // CONFIGURACIÓN DE PINES GPIO
  // ============================================
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(IR_SENSOR_PIN, INPUT_PULLUP);
  pinMode(ISD1820_PLAY_PIN, OUTPUT);
  pinMode(FLASH_LED_PIN, OUTPUT);
  
  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(ISD1820_PLAY_PIN, LOW);
  digitalWrite(FLASH_LED_PIN, LOW);
  
  Serial.println("Pines GPIO configurados");

  // ============================================
  // CONFIGURACIÓN DE WIFI CON WIFI MANAGER
  // ============================================
  WiFiManager wm;
  
  // Configurar nombre del AP
  wm.setConfigPortalTimeout(180);  // 3 minutos de timeout
  
  // Intentar conectar con credenciales guardadas
  bool res = wm.autoConnect("ESP32-CAM-Config", "12345678");
  
  if (!res) {
    Serial.println("No se pudo conectar a WiFi. Iniciando portal de configuración...");
    Serial.println("Conéctate a 'ESP32-CAM-Config' con password '12345678'");
    Serial.println("Luego accede a http://192.168.4.1");
  } else {
    Serial.println("WiFi conectado exitosamente!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    
    // Guardar credenciales actuales
    strncpy(wifiSSID, WiFi.SSID().c_str(), 32);
    strncpy(wifiPassword, WiFi.psk().c_str(), 64);
    saveConfig();
  }

  // ============================================
  // INICIALIZACIÓN DE LA CÁMARA
  // ============================================
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
  config.pixel_format = PIXFORMAT_JPEG;
  
  // Resolución más baja para mejor rendimiento en detección de movimiento
  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 10;
  config.fb_count = 2;

  // Iniciar cámara
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Error iniciando cámara: 0x%x", err);
    return;
  }

  Serial.println("Cámara iniciada correctamente");

  // ============================================
  // INICIAR CLIENTE NTP
  // ============================================
  if (WiFi.status() == WL_CONNECTED) {
    timeClient.begin();
    timeClient.setTimeOffset(timezoneOffset);
    Serial.println("Cliente NTP iniciado");
  }

  // ============================================
  // CONFIGURACIÓN DEL SERVIDOR WEB
  // ============================================
  server.on("/", HTTP_GET, handleRoot);
  server.on("/capture", HTTP_GET, handleCapture);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/time", HTTP_GET, handleTime);
  server.on("/relay", HTTP_GET, handleRelay);
  server.on("/mode", HTTP_GET, handleMode);
  server.on("/audio", HTTP_GET, handleAudio);
  server.on("/reset", HTTP_GET, handleReset);
  server.on("/schedule", HTTP_POST, handleSchedule);
  server.on("/flash", HTTP_GET, handleFlash);
  server.on("/flashmode", HTTP_GET, handleFlashMode);
  server.on("/flashschedule", HTTP_POST, handleFlashSchedule);
  server.on("/timezone", HTTP_POST, handleTimezone);
  server.on("/wifi", HTTP_POST, handleWiFi);
  
  server.begin();
  Serial.println("Servidor web iniciado");
  Serial.println("Sistema listo - esperando perro...");
}

void loop() {
  server.handleClient();
  
  // Actualizar máquina de estados
  updateStateMachine();
  
  delay(10);  // Pequeño delay para no saturar el CPU
}
