# Hardware Notes: Seeed Studio XIAO ESP32-S3 Sense

## Critical Hardware Limitations

### ❌ NO Bluetooth Classic Support
**The ESP32-S3 chip does NOT support Bluetooth Classic, only BLE (Bluetooth Low Energy).**

- **This means**: No A2DP, no Bluetooth audio streaming
- **Reason**: Hardware limitation - the S3 chip lacks the Bluetooth Classic radio
- **Only ESP32 (original)** supports Bluetooth Classic and A2DP

### Audio Capabilities

**Input (✅ Available)**
- PDM Microphone: MSM261D3526H1CPM
- I2S Pins: GPIO 42 (Clock), GPIO 41 (Data)
- 16kHz, 16-bit mono

**Output (❌ Not Built-in)**
- No speaker on board
- No audio DAC on board
- No Bluetooth Classic for A2DP audio

## Audio Output Solutions for POC

Since the ESP32-S3 Sense cannot do Bluetooth audio, we have these options:

### Option 1: WebSocket Audio Streaming (Recommended for POC)
Stream synthesized audio back to the computer/phone and play it there.
- Pro: No additional hardware needed
- Pro: Works immediately for testing
- Con: Requires computer/phone for audio playback

### Option 2: External I2S Amplifier
Connect an I2S audio amplifier module (e.g., MAX98357A) to GPIO pins.
- Pro: Standalone audio output
- Con: Requires additional hardware
- Con: More complex setup

### Option 3: WiFi Speaker
Use a WiFi-enabled speaker that accepts audio streams.
- Pro: Wireless audio
- Con: Requires specific speaker hardware
- Con: More complex protocol

## Recommended POC Approach

For the Voice POC, use **WebSocket audio streaming** back to the computer:
1. ESP32 captures audio via PDM mic
2. Streams to server via WebSocket
3. Server processes (STT → LLM → TTS)
4. Server plays TTS audio through computer speakers
5. Or server streams audio back to a phone app

This avoids the Bluetooth Classic limitation entirely.