# Changelog

Todos los cambios importantes del proyecto se documentan en este archivo.

## [1.7.0] - 2026-04-17

### Añadido
- **Timeout automático del flash**: se apaga después de 10 segundos sin movimiento detectado
- **Registro de tiempo fuera del perro**:
  - Variable `dogExitTime` para registrar momento de salida
  - Variable `dogOutsideDuration` para calcular duración en segundos
  - Muestra tiempo en Monitor Serie al regresar el perro
  - Campo `dog_outside_duration` en endpoint `/status`
  - Visualización en interfaz web: "Tiempo fuera (último): Xs"
- **Relay apagado por defecto**: `setRelay(false)` explícito al inicio del sistema

### Corregido
- **Modo manual del flash**: ahora respeta el modo manual y no se enciende automáticamente con movimiento
- Solo se enciende con movimiento si `flashAutoMode` está activado

## [1.6.0] - 2026-04-16

### Añadido
- **Filtros avanzados de detección de movimiento** configurables desde la interfaz web:
  - Umbral de diferencia de píxeles (0-255)
  - Área mínima de movimiento (píxeles)
  - Área máxima de movimiento (píxeles)
  - Frames consecutivos requeridos
  - Porcentaje máximo de píxeles diferentes (0-100%)
- Endpoint `/motionconfig` (POST) para guardar configuración de movimiento
- Persistencia de filtros de movimiento en SPIFFS
- Lógica de filtrado en `detectMotion()` para reducir falsos positivos:
  - Ignora movimientos muy pequeños (< motionMinArea)
  - Ignora movimientos muy grandes (> motionMaxArea)
  - Requiere detección en N frames consecutivos (motionMinFrames)
  - Usa porcentaje configurable de píxeles diferentes
- Sección de solución de problemas para ajustar sensibilidad de detección

### Cambiado
- Mejorada precisión de detección de movimiento con filtros configurables
- Valores por defecto: threshold=30, minArea=50, maxArea=5000, minFrames=2, maxPercentage=1

## [1.5.0] - 2026-04-16

### Añadido
- **Factory Reset**: Mantener presionado botón BOOT (GPIO0) por 3 segundos durante el arranque borra toda la configuración (WiFi + SPIFFS)
- Borrado completo de credenciales WiFi del NVS usando `WiFiManager.resetSettings()`

### Cambiado
- Reemplazado método de doble reset por BOOT+RESET (más confiable)
- Credenciales WiFi por defecto ahora vacías (se configuran vía WiFiManager)

### Eliminado
- Dependencia de `Preferences` (NVS) para contador de reset
- Variables `RTC_DATA_ATTR` para detección de doble reset

## [1.4.0] - 2026-04-16

### Añadido
- **WiFi Manager**: Portal de configuración WiFi al iniciar sin credenciales
  - AP "ESP32-CAM-Config" con password "12345678"
  - Portal en http://192.168.4.1
- Configuración manual de WiFi desde la interfaz web (`/wifi` endpoint)
- Persistencia de credenciales WiFi en SPIFFS

## [1.3.0] - 2026-04-16

### Añadido
- **Persistencia de configuración** con SPIFFS:
  - Zona horaria
  - Horario del flash (inicio/fin)
  - Credenciales WiFi
- Carga automática de configuración al inicio
- Archivo `/config.txt` en SPIFFS

### Corregido
- Parsing del offset de zona horaria (ignorar comillas de JSON)
- Zona horaria por defecto cambiada a UTC-8 (Los Ángeles)

## [1.2.0] - 2026-04-16

### Añadido
- **Configuración de zona horaria** desde interfaz web
- Endpoint `/timezone` (POST)
- Variable global `timezoneOffset`

## [1.1.0] - 2026-04-16

### Añadido
- **Control del Flash LED** (GPIO4):
  - Activación automática por detección de movimiento
  - Modo manual con toggle desde web
  - Horario de rango de activación configurable (ej: 18:00 - 07:00)
- Endpoints `/flash`, `/flashmode`, `/flashschedule`

## [1.0.0] - 2026-04-16

### Añadido
- Sistema completo de detección de perro
- Máquina de estados (esperando perro → detectado → salida → retorno)
- Control de Relay (GPIO12)
- Sensor Infrarrojo (GPIO13)
- Módulo ISD1820 para audio (GPIO14)
- Cliente NTP para sincronización horaria
- Horarios programados de audio (8, 12, 18, 21)
- Interfaz web de administración:
  - Estado del sistema en tiempo real
  - Control manual de relay
  - Modo automático/manual
  - Vista previa de cámara
  - Reproducción manual de audio
  - Configuración de horarios
- API REST para control del sistema
