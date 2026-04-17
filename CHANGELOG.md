# Changelog

Todos los cambios importantes del proyecto se documentan en este archivo.

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
