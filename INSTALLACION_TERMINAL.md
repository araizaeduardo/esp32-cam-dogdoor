# Instalación y Uso desde Terminal Linux

## 1. Instalar PlatformIO CLI

```bash
# Instalar pip si no lo tienes
sudo apt update
sudo apt install python3-pip

# Instalar PlatformIO CLI
pip3 install platformio

# Verificar instalación
pio --version
```

## 2. Compilar el proyecto

```bash
cd /home/eduardo/Desktop/esp32-cam
pio run
```

## 3. Cargar el código al ESP32-CAM

```bash
# Compilar y cargar
pio run --target upload

# O solo cargar si ya está compilado
pio run --target upload
```

## 4. Ver el monitor serie

```bash
pio device monitor
```

## Comandos útiles

```bash
# Limpiar archivos de compilación
pio run --target clean

# Ver puertos disponibles
pio device list

# Compilar y cargar en un solo comando
pio run --target upload && pio device monitor
```

## Configuración del puerto

Si PlatformIO no detecta automáticamente el puerto, especifica:

```bash
pio run --target upload --upload-port /dev/ttyUSB0
```

## Notas importantes

- Asegúrate de que GPIO0 esté conectado a GND antes de cargar
- Después de cargar, desconecta GPIO0 de GND y presiona RESET
- El puerto puede ser /dev/ttyUSB0, /dev/ttyUSB1, /dev/ttyACM0, etc.
