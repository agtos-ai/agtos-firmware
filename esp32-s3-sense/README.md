# ESP32-S3 Sense Voice Firmware

## Current Status

This is the **working POC firmware** for the XIAO ESP32-S3 Sense that successfully:
- ✅ Connects to WiFi and WebSocket server
- ✅ Initializes PDM microphone with noise floor compensation
- ✅ Streams audio over WebSocket as PCM
- ✅ Handles voice commands (start/stop recording)

**Note**: This is Arduino-based POC code. Production firmware will be rewritten in ESP-IDF for better performance and control.

## Hardware Requirements

- **Device**: Seeed Studio XIAO ESP32-S3 Sense (Pre-Soldered)
- **Onboard Microphone**: MSM261D3526H1CPM (PDM)
- **Connection**: USB-C cable

## Software Requirements

### Arduino IDE Setup

1. **Install Arduino IDE** (2.0 or later)
   - Download from: https://www.arduino.cc/en/software

2. **Add ESP32 Board Support**
   - Open Arduino IDE
   - Go to `File → Preferences`
   - Add to "Additional Board Manager URLs":
     ```
     https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
     ```
   - Go to `Tools → Board → Board Manager`
   - Search for "esp32" and install "esp32 by Espressif Systems" (v2.0.11 or later)

3. **Select Board**
   - Go to `Tools → Board → esp32 → XIAO_ESP32S3`

4. **Install Required Libraries**
   - Go to `Tools → Manage Libraries`
   - Install:
     - `WebSockets` by Markus Sattler
     - `ArduinoJson` by Benoit Blanchon
     - Note: I2S support is built into ESP32 Arduino Core (no separate library needed)

## Configuration

1. **Update WiFi Credentials** in `esp32-s3-sense.ino`:
   ```cpp
   const char* WIFI_SSID = "YOUR_WIFI_SSID";
   const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
   ```

2. **Update Server IP**:
   ```cpp
   const char* WEBSOCKET_HOST = "192.168.1.100";  // Your computer's IP
   ```
   
   To find your computer's IP:
   - **Linux/Mac**: `ifconfig` or `ip addr`
   - **Windows**: `ipconfig`

## Flashing the Firmware

1. **Connect ESP32-S3 Sense** via USB-C cable

2. **Select Port**
   - Go to `Tools → Port`
   - Select the port (e.g., `/dev/ttyUSB0` on Linux, `COM3` on Windows)

3. **Upload Settings**:
   - Upload Speed: 921600
   - USB CDC On Boot: Enabled
   - USB Mode: Hardware CDC and JTAG

4. **Upload**
   - Click the Upload button (→)
   - Wait for "Done uploading"

5. **Monitor Serial Output**
   - Open `Tools → Serial Monitor`
   - Set baud rate to 115200
   - You should see:
     ```
     ========================================
     agtOS Voice Firmware v1.0
     Device: XIAO ESP32-S3 Sense
     ========================================
     
     Connecting to WiFi: YOUR_SSID
     ✅ WiFi connected!
     IP address: 192.168.1.x
     ✅ I2S initialized successfully
     ✅ WebSocket connected
     ```

## Testing

### Server-Side Setup

1. **Start Docker Services**:
   ```bash
   cd /workspace
   docker-compose -f docker/docker-compose.voice.yml up
   ```

2. **Monitor Logs**:
   ```bash
   docker-compose -f docker/docker-compose.voice.yml logs -f agtos-voice
   ```

### Test Commands

Once connected, the server can send commands:

- `start_recording` - Begin audio capture
- `stop_recording` - Stop audio capture
- `status` - Get device status
- `ping` - Connectivity test

### LED Indicators (if implemented)

- **Blue LED**: Connected to WiFi
- **Green LED**: Recording active
- **Red LED**: Error state

## Troubleshooting

### WiFi Connection Issues
- Verify SSID and password
- Check 2.4GHz network (ESP32 doesn't support 5GHz)
- Ensure router allows new device connections

### WebSocket Connection Issues
- Verify server IP address
- Check firewall settings
- Ensure server is running on port 3000

### Audio Issues
- Microphone is PDM type, requires specific I2S configuration
- Sample rate must be 16kHz for stability
- Check serial monitor for I2S initialization errors

### Compilation Issues
- **"I2S.h: No such file or directory"** - This firmware uses the native ESP32 I2S driver (`driver/i2s.h`), not the Arduino I2S library
- Make sure ESP32 board package is v2.0.11 or later
- The PDM microphone requires ESP32-specific I2S configuration not available in generic Arduino libraries

### USB Passthrough to Docker

If running server in Docker, expose the ESP32 USB device:

```bash
# Linux
docker run --device=/dev/ttyUSB0 ...

# macOS
docker run --device=/dev/cu.usbserial-0001 ...

# Or use privileged mode (less secure)
docker run --privileged ...
```

## Audio Data Format

The ESP32 sends audio in the following format:
- **Format**: PCM (raw audio)
- **Sample Rate**: 16,000 Hz
- **Bit Depth**: 16-bit
- **Channels**: 1 (Mono)
- **Byte Order**: Little-endian

### PDM Microphone Technical Details
The MSM261D3526H1CPM is a PDM (Pulse Density Modulation) MEMS microphone:
- **Interface**: PDM digital output
- **Clock Pin**: GPIO 42 (provides clock to microphone)
- **Data Pin**: GPIO 41 (receives PDM data)
- **Frequency Response**: 50Hz - 20kHz
- **SNR**: 61dB
- **Sensitivity**: -26dBFS

The ESP32's I2S peripheral automatically converts PDM to PCM internally when configured in PDM mode.

Server must convert this to WAV format for Whisper:
```javascript
// Add WAV header to PCM data
function pcmToWav(pcmData, sampleRate = 16000) {
  // WAV header construction...
}
```

## Development Notes

### Memory Optimization
- ESP32-S3 has 8MB Flash, 8MB PSRAM
- Audio buffer size affects latency vs stability
- Larger buffers = more stable, higher latency

### Power Consumption
- WiFi + I2S active: ~120mA
- Consider deep sleep when not in use
- USB power is sufficient for development

### Future Enhancements
- [ ] Add wake word detection
- [ ] Implement voice activity detection (VAD)
- [ ] Add status LED control
- [ ] Support OTA updates
- [ ] Add camera support (OV2640 onboard)

## Resources

- [XIAO ESP32S3 Wiki](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/)
- [ESP32 I2S Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/i2s.html)
- [WebSockets Library](https://github.com/Links2004/arduinoWebSockets)
- [ArduinoJson](https://arduinojson.org/)