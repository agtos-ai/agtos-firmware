<!--
Provenance: extracted from the agtOS repository (agtos-ai/agtos) at commit
c063025e on 2026-08-18, where it lived at `test/ESP32_VOICE_TEST_PLAN.md`.
agtOS removed its dead `test/` scratch directory (agtos-ai/agtos issue 1444);
this document describes ESP32 hardware bring-up, so it belongs with the
firmware it tests rather than with the server. Paths and ports inside refer
to that 2025-era agtOS test stack and are historical.
-->

# ESP32-S3 Sense Voice Testing & Implementation Plan

## 🎯 Objective
Verify ESP32 hardware functionality first, then build production voice infrastructure on proven foundation.

## 📋 Testing Phases

### Phase 1: Hardware Verification (Immediate)

#### 1.1 Start Test Server
```bash
cd /workspace/test
npm install ws
npm start

# Server will listen on:
# - WebSocket: ws://0.0.0.0:3000/ws/audio
# - Web UI: http://localhost:3000
```

#### 1.2 Monitor ESP32 Serial Output
```bash
# In another terminal
screen /dev/cu.usbmodem2101 115200

# Expected output:
# ========================================
# agtOS Voice Firmware v1.0
# Device: XIAO ESP32-S3 Sense
# ========================================
# 
# Connecting to WiFi: bobot
# ✅ WiFi connected!
# IP address: 192.168.3.x
# ✅ I2S initialized successfully
# ✅ WebSocket connected
```

#### 1.3 Test Connection
1. Open http://localhost:3000 in browser
2. Should see "ESP32 connected" in log
3. Device info should appear with capabilities

#### 1.4 Test Audio Streaming
1. Click "Start Recording" in web UI
2. Speak into ESP32 microphone
3. Watch audio bytes counter increase
4. Click "Stop Recording"
5. Check `/workspace/test/audio-samples/` for PCM file

#### 1.5 Verify Audio Quality
```bash
# Convert PCM to WAV for playback
cd /workspace/test
node convert-pcm-to-wav.js

# Play the WAV file to verify quality
# Should hear clear audio at 16kHz mono
```

### Phase 2: Protocol Verification

#### 2.1 Command Testing
Test each command via web UI:
- `start_recording` - Begin audio capture
- `stop_recording` - Stop audio capture  
- `status` - Get device status
- `ping` - Test connectivity

#### 2.2 Audio Format Validation
- Verify PCM format: 16-bit, 16kHz, mono, little-endian
- Check packet sizes and intervals
- Measure latency and jitter

#### 2.3 Stress Testing
- Long recording sessions (5+ minutes)
- Rapid start/stop cycles
- Network interruption recovery
- Memory leak detection

### Phase 3: Infrastructure Implementation

#### 3.1 Refactor Architecture (Day 1)
```typescript
// Split monolithic orchestrator
src/
  protocols/
    voice/
      infrastructure.ts    // Technical pipeline
      orchestrator.ts      // Business logic
      websocket-handler.ts // ESP32 connection
      audio-converter.ts   // PCM to WAV
```

#### 3.2 Docker Integration (Day 2)
```bash
# Start real services
docker-compose -f docker/docker-compose.voice.yml up

# Connect infrastructure to Docker services
# - Whisper on port 9000
# - Piper on port 5003
```

#### 3.3 Claude CLI Integration (Day 3)
```typescript
// Implement CLI wrapper
class ClaudeCodeCLI {
  async process(input: string): Promise<string> {
    // Spawn claude process
    // Handle streaming responses
  }
}
```

#### 3.4 End-to-End Testing (Day 4)
Complete voice flow:
1. ESP32 → WebSocket → Server
2. PCM → WAV conversion
3. Whisper transcription
4. Claude processing
5. Piper TTS
6. Response to ESP32

## 🔧 Troubleshooting Guide

### ESP32 Won't Connect

#### WiFi Issues
- Verify SSID/password in firmware
- Check 2.4GHz network (not 5GHz)
- Ensure router allows new devices
- Check WiFi signal strength

#### WebSocket Issues
- Verify server IP in firmware matches computer
- Check firewall allows port 3000
- Ensure server is running before ESP32 boots
- Try restarting ESP32 after server starts

### No Audio Data

#### Hardware Issues
- Check ESP32 has power (LED indicators)
- Verify USB connection is stable
- Try different USB cable/port
- Check microphone isn't blocked

#### Software Issues
- Verify I2S initialized in serial output
- Check recording started successfully
- Monitor WebSocket for binary frames
- Verify buffer sizes adequate

### Poor Audio Quality

#### Common Causes
- Incorrect sample rate (must be 16kHz)
- Bit depth mismatch (expects 16-bit)
- Endianness issues (little-endian required)
- Packet loss or corruption

#### Solutions
- Increase DMA buffer size
- Reduce network traffic
- Move ESP32 closer to router
- Shield from electromagnetic interference

## 📊 Success Metrics

### Phase 1 (Hardware Test)
- [ ] ESP32 connects to WiFi
- [ ] WebSocket connection established
- [ ] Audio streams successfully
- [ ] Commands work bidirectionally
- [ ] Audio quality acceptable

### Phase 2 (Protocol Test)
- [ ] All commands function correctly
- [ ] Audio format validated
- [ ] No memory leaks detected
- [ ] Network recovery works
- [ ] 10-minute recording stable

### Phase 3 (Infrastructure)
- [ ] Architecture refactored
- [ ] Docker services integrated
- [ ] Claude CLI functional
- [ ] End-to-end flow works
- [ ] < 3 second response time

## 🚀 Quick Start Commands

```bash
# Terminal 1: Start test server
cd /workspace/test
npm install && npm start

# Terminal 2: Monitor ESP32
screen /dev/cu.usbmodem2101 115200

# Terminal 3: Watch audio files
watch -n 1 'ls -lah /workspace/test/audio-samples/'

# Browser: Open monitoring UI
open http://localhost:3000

# Convert audio after recording
node convert-pcm-to-wav.js

# Play converted audio (macOS)
afplay audio-samples/recording_*.wav
```

## 📝 Implementation Checklist

### Today (Hardware Verification)
- [x] Create test server
- [x] Create audio converter
- [x] Document test plan
- [ ] Start server and connect ESP32
- [ ] Record test audio
- [ ] Verify audio quality
- [ ] Document any issues

### Tomorrow (Architecture)
- [ ] Create GitHub issue for refactoring
- [ ] Implement VoiceInfrastructure class
- [ ] Implement VoiceOrchestrator class
- [ ] Create WebSocket handler
- [ ] Test with ESP32

### Day 3 (Integration)
- [ ] Connect to Whisper Docker
- [ ] Connect to Piper Docker
- [ ] Implement Claude CLI wrapper
- [ ] Create audio converter utility

### Day 4 (Testing)
- [ ] End-to-end voice test
- [ ] Performance optimization
- [ ] Error handling
- [ ] Documentation

## 🎉 Next Steps After Success

Once basic voice works:
1. Add voice activity detection (VAD)
2. Implement wake word detection
3. Add speaker support to ESP32
4. Create MCP server wrapper
5. Support multiple devices
6. Add S2S model support

## 💡 Key Insights

1. **Test Hardware First**: Never assume hardware works - verify everything
2. **Incremental Validation**: Test each layer independently before integration
3. **Real Data Matters**: Actual audio reveals issues mocks never would
4. **Simple First**: Basic WebSocket before WebRTC, PCM before compression
5. **Document Everything**: Every issue and solution helps future debugging

---

**Remember**: We're building production architecture from day 1, just testing it incrementally. Every test informs the final design.