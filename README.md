# ESP32-S3 Voice Assistant Firmware

## Firmware Versions

### 1. Basic Version (PDM Microphone)
**Location**: `/firmware/esp32-s3-sense/esp32-s3-sense.ino`

- Uses built-in PDM microphone on XIAO ESP32-S3 Sense
- Audio output plays on computer speakers
- No additional hardware required
- Wake word: "Hey chichi"

**To use**:
1. Open in Arduino IDE: `firmware/esp32-s3-sense/esp32-s3-sense.ino`
2. Update WiFi credentials and server IP in code
3. Upload to ESP32-S3 Sense
4. Audio plays through computer speakers

### 2. ReSpeaker Enhanced Version
**Location**: `/firmware/esp32-s3-sense-respeaker/esp32-s3-sense-respeaker.ino`

- Uses ReSpeaker Lite's dual microphones via I2S
- Hardware DSP for better audio quality
- Audio output via ReSpeaker's headphone jack
- Wake word: "Hey chichi"

**To use**:
1. Open in Arduino IDE: `firmware/esp32-s3-sense-respeaker/esp32-s3-sense-respeaker.ino`
2. Connect ReSpeaker Lite to XIAO ESP32S3
3. Update WiFi credentials and server IP in code
4. Upload to ESP32-S3 Sense
5. Connect headphones to ReSpeaker

## Hardware Notes

### Seeed Studio XIAO ESP32-S3 Sense
- **Bluetooth**: BLE only (NO Bluetooth Classic/A2DP)
- **Microphone**: MSM261D3526H1CPM (PDM)
- **I2S Pins**: GPIO 42 (Clock), GPIO 41 (Data)

### Why No Bluetooth Audio?
The ESP32-S3 chip does NOT support Bluetooth Classic, which is required for A2DP audio streaming. This is a hardware limitation. For wireless audio, you would need:
- Original ESP32 (has Bluetooth Classic)
- External Bluetooth module (CSR8675, BC127, etc.)
- Or use the ReSpeaker with headphone jack

## Server Requirements

Run the voice conversation server:
```bash
node test/voice-conversation-server.js
```

Access the WebUI at: http://localhost:8765

## Arduino IDE Setup

1. **Board**: XIAO_ESP32S3
2. **Libraries Required**:
   - WebSocketsClient
   - ArduinoJson
   - ESP_I2S (comes with ESP32 board package)

## Important Configuration

Before uploading, update these values in the firmware:

```cpp
const char* WIFI_SSID = "YourWiFiName";
const char* WIFI_PASSWORD = "YourWiFiPassword";
const char* WEBSOCKET_HOST = "192.168.X.X";  // Your computer's IP
```