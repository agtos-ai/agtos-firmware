# ESP32 Voice Device Firmware

Firmware for agtOS-compatible ESP32 voice devices with wake word detection and WebRTC audio streaming.

## Features

- **Wake Word Detection**: Energy-based VAD with "hey chichi" wake word
- **WebRTC Audio Streaming**: Real-time audio to agtOS server
- **I2S Microphone Support**: INMP441 digital microphone
- **OTA Updates**: Over-the-air firmware updates
- **LED Indicators**: Visual feedback for WiFi, wake word, and streaming
- **Button Trigger**: Manual wake word trigger via boot button

## Hardware Requirements

- ESP32 DevKit (ESP32-WROOM-32 or similar)
- INMP441 I2S Digital Microphone
- 3 LEDs (optional): Blue (WiFi), Green (Wake), Red (Active)
- Jumper wires

## Pin Connections

### INMP441 Microphone
- **WS** (Word Select) → GPIO 15
- **SD** (Serial Data) → GPIO 32  
- **SCK** (Serial Clock) → GPIO 14
- **L/R** → GND (Left channel)
- **VDD** → 3.3V
- **GND** → GND

### LED Indicators (Optional)
- **Blue LED** (WiFi) → GPIO 2
- **Green LED** (Wake) → GPIO 4
- **Red LED** (Active) → GPIO 5
- All LEDs need current-limiting resistors (330Ω)

### Button
- **Boot Button** (GPIO 0) - Built-in, used for manual trigger

## Building the Firmware

### Prerequisites

1. Install ESP-IDF v5.0+:
```bash
# Install prerequisites
sudo apt-get install git wget flex bison gperf python3 python3-venv cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0

# Clone ESP-IDF
mkdir -p ~/esp
cd ~/esp
git clone -b v5.0 --recursive https://github.com/espressif/esp-idf.git

# Install ESP-IDF
cd ~/esp/esp-idf
./install.sh esp32

# Source the environment
. ./export.sh
```

2. Configure WiFi credentials:
```bash
cd /workspace/firmware/esp32-voice
idf.py menuconfig
# Navigate to: Component config → WiFi Configuration
# Set SSID and Password
```

### Build

```bash
# Clean build
idf.py fullclean

# Configure target
idf.py set-target esp32

# Build firmware
idf.py build
```

### Flash

```bash
# Flash firmware and monitor output
idf.py -p /dev/ttyUSB0 flash monitor

# Or flash only
idf.py -p /dev/ttyUSB0 flash

# Monitor only
idf.py -p /dev/ttyUSB0 monitor
```

Replace `/dev/ttyUSB0` with your ESP32's serial port (e.g., `/dev/ttyUSB0` on Linux, `COM3` on Windows).

## PlatformIO Alternative

If you prefer PlatformIO:

```bash
# Install PlatformIO
pip install platformio

# Build
pio run

# Upload
pio run -t upload

# Monitor
pio run -t monitor

# Upload and monitor
pio run -t upload -t monitor
```

## Configuration

### Runtime Configuration

The device stores configuration in NVS flash. Use the serial console commands:

```
# Set wake word
config set wake_word "hey assistant"

# Set server URL  
config set server_url "ws://192.168.1.100:3000/webrtc"

# Show current config
config show
```

### Build-time Configuration

Edit `sdkconfig.defaults`:

```makefile
CONFIG_WIFI_SSID="YourWiFiSSID"
CONFIG_WIFI_PASSWORD="YourWiFiPassword"
CONFIG_DEVICE_ID="esp32-voice-001"
CONFIG_WEBRTC_SERVER="ws://192.168.1.100:3000/webrtc"
CONFIG_WAKE_WORD="hey_chichi"
```

## OTA Updates

The device supports over-the-air updates:

1. Build new firmware
2. Host the binary on a web server
3. Trigger update via WebSocket command or button press

## LED Indicators

- **Blue (WiFi)**:
  - Blinking: Connecting to WiFi
  - Solid: Connected to WiFi
  - Off: No WiFi connection

- **Green (Wake)**:
  - On: Wake word detected
  - Off: Idle

- **Red (Active)**:
  - On: Streaming audio
  - Off: Not streaming

## Troubleshooting

### No Audio Input
- Check INMP441 connections
- Verify I2S pins in code match hardware
- Check microphone power (3.3V)

### WiFi Connection Issues
- Verify SSID and password
- Check router compatibility (2.4GHz only)
- Increase connection timeout

### WebSocket Connection Failed
- Verify server URL and port
- Check firewall settings
- Ensure server is running

### High Noise/Static
- Add capacitors near microphone power
- Use shorter wires
- Keep away from WiFi antenna

## Memory Usage

- **Flash**: ~1.3MB (includes OTA partitions)
- **RAM**: ~80KB (with audio buffers)
- **PSRAM**: Optional, improves performance

## Performance

- **Wake Word Latency**: <100ms
- **Audio Streaming**: 16kHz, 16-bit PCM
- **WebSocket Latency**: <50ms local network
- **Power Consumption**: ~150mA active, ~80mA idle

## License

Part of agtOS - Your AI, Your Way