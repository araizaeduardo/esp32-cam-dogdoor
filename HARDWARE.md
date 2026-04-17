# Sistema de Detección de Perro - Conexiones de Hardware

## Componentes Necesarios

- ESP32-CAM (AI Thinker)
- Módulo Relay (5V)
- Sensor Infrarrojo (PIR o barrera IR)
- Módulo de Audio ISD1820
- Fuente de alimentación 5V/2A (recomendada)

## Diagrama de Conexiones

### ESP32-CAM a Módulo Relay
```
ESP32-CAM GPIO12 ──> Relay IN
ESP32-CAM GND    ──> Relay GND
ESP32-CAM 3.3V   ──> Relay VCC (si el relay es de 3.3V)
                   ──> O usa 5V externo si el relay es de 5V
```

**Nota**: Si el relay es de 5V, alimenta el VCC del relay con 5V externo, no con el 3.3V del ESP32-CAM. Común GND entre ESP32 y relay.

### ESP32-CAM a Sensor Infrarrojo
```
ESP32-CAM GPIO13 ──> IR Sensor OUT (o VCC para algunos sensores)
ESP32-CAM GND    ──> IR Sensor GND
ESP32-CAM 3.3V   ──> IR Sensor VCC
```

**Nota**: El código asume sensor IR activo bajo (LOW cuando detecta). Si tu sensor es activo alto, modifica la función `readIRSensor()`.

### ESP32-CAM a Módulo ISD1820
```
ESP32-CAM GPIO14 ──> ISD1820 PLAY
ESP32-CAM GND    ──> ISD1820 GND
ESP32-CAM 3.3V   ──> ISD1820 VCC
```

**Nota**: El ISD1820 puede funcionar con 3.3V, pero para mejor calidad de audio usa 5V externo.

## Configuración del ISD1820

El módulo ISD1820 necesita que grabes el mensaje de audio antes de usarlo:

1. **Modo de grabación**:
   - Mantén presionado el botón REC
   - Habla el mensaje (ej: "Sal a jugar")
   - Suelta el botón REC

2. **Reproducción**:
   - El ESP32-CAM enviará un pulso HIGH al pin PLAY (GPIO14)
   - El módulo reproducirá el mensaje grabado
   - Duración máxima de grabación: ~20 segundos (depende del modelo)

## Sensor Infrarrojo

### Opción 1: Sensor PIR (Movimiento)
```
PIR VCC ──> 3.3V o 5V
PIR GND ──> GND
PIR OUT ──> GPIO13
```
- Detecta movimiento en un área
- Ideal para detectar cuando el perro pasa por una puerta

### Opción 2: Barrera IR (Punto a punto)
```
Emisor IR: VCC + GND
Receptor IR OUT ──> GPIO13
Receptor IR GND ──> GND
```
- Detecta cuando el perro cruza la barrera
- Más preciso para entrada/salida

## Alimentación

### Recomendación
Usa una fuente de 5V/2A externa para:
- ESP32-CAM (pin 5V)
- Módulo Relay
- Sensor IR
- ISD1820

### Conexión de alimentación
```
Fuente 5V ──> ESP32-CAM 5V
Fuente 5V ──> Relay VCC
Fuente 5V ──> ISD1820 VCC (opcional)
Fuente GND ──> ESP32-CAM GND
Fuente GND ──> Relay GND
Fuente GND ──> IR Sensor GND
Fuente GND ──> ISD1820 GND
```

**Importante**: Todos los GND deben estar conectados juntos (común ground).

## Pines Disponibles en ESP32-CAM (AI Thinker)

Pines que NO usan la cámara:
- GPIO2, GPIO4, GPIO12, GPIO13, GPIO14, GPIO15, GPIO16, GPIO17, GPIO33

Pines usados en este proyecto:
- GPIO12: Relay
- GPIO13: Sensor IR
- GPIO14: ISD1820 PLAY

Pines reservados para cámara (NO usar):
- GPIO0, GPIO5, GPIO18, GPIO19, GPIO21, GPIO22, GPIO23, GPIO25, GPIO26, GPIO27, GPIO32, GPIO34, GPIO35, GPIO36, GPIO39

## Configuración de Horarios

Edita estas líneas en `src/main.cpp`:

```cpp
const int AUDIO_HOURS[] = {8, 12, 18, 21};  // Horas para reproducir audio
const int NUM_AUDIO_HOURS = 4;
```

Zona horaria (línea 71):
```cpp
NTPClient timeClient(ntpUDP, "pool.ntp.org", -18000, 60000);
// -18000 = UTC-5 (México)
// -21600 = UTC-6 (Centroamérica)
// 3600 = UTC+1 (Europa)
```

## Flujo del Sistema

1. **Esperando perro**: Cámara detecta movimiento → Activa relay
2. **Perro detectado**: Espera que sensor IR detecte salida
3. **Esperando salida**: IR detecta → Relay sigue activo
4. **Perro fuera**: Espera retorno (IR + cámara)
5. **Esperando retorno**: IR detecta → Cámara confirma → Desactiva relay
6. **Perro regresó**: Ciclo completado, vuelve a paso 1

**Audio programado**: A las horas configuradas, reproduce mensaje si está en estado "Esperando perro"

## Solución de Problemas

### Relay no funciona
- Verifica que el relay esté alimentado con voltaje correcto
- Comprueba que el GND sea común entre ESP32 y relay
- Prueba con otro pin GPIO

### Sensor IR no detecta
- Verifica la polaridad del sensor
- Ajusta la sensibilidad si es un PIR (potenciómetro)
- Modifica `readIRSensor()` si es activo alto

### ISD1820 no reproduce
- Verifica que el mensaje esté grabado
- Prueba reproducción manual con el botón PLAY
- Aumenta la duración del pulso en `playAudio()` (línea 143)

### Cámara no detecta movimiento
- Ajusta el umbral en `detectMotion()` (línea 98)
- Mejora la iluminación del área
- Reduce la resolución si el sistema es lento

## Web Interface

Accede a:
- `http://IP_ESP32/` - Estado del sistema
- `http://IP_ESP32/capture` - Ver imagen de la cámara
- `http://IP_ESP32/status` - JSON con estado actual
