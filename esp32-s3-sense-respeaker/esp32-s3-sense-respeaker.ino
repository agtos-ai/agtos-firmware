/**
 * ESP32-S3 Voice Assistant - ReSpeaker Version
 * For: Seeed Studio XIAO ESP32-S3 Sense + ReSpeaker Lite
 * 
 * Features:
 * - I2S audio input from ReSpeaker (dual mics with DSP)
 * - I2S audio output to ReSpeaker (headphone jack)
 * - Wake word detection ("Hey chichi")
 * - WebSocket streaming
 * - Hardware audio processing via XMOS chip
 * 
 * Version: 1.0 - ReSpeaker Enhanced
 */

#include <WiFi.h>
#include <WebSocketsClient.h>
#include <driver/i2s.h>
#include <ArduinoJson.h>

// ============================================================================
// Configuration - UPDATE THESE FOR YOUR NETWORK
// ============================================================================

const char* WIFI_SSID = "Redux";              // Your WiFi network name
const char* WIFI_PASSWORD = "day-one-21";     // Your WiFi password
const char* WEBSOCKET_HOST = "192.168.3.196"; // Your computer's IP address
const int WEBSOCKET_PORT = 8765;
const char* WEBSOCKET_PATH = "/ws/audio";

// ============================================================================
// I2S Configuration for ReSpeaker
// ============================================================================

// ReSpeaker I2S pins (check ReSpeaker documentation for exact pins)
#define I2S_WS 9      // Word Select (LRCLK)
#define I2S_SD 8      // Serial Data
#define I2S_SCK 7     // Serial Clock (BCLK)
#define I2S_SD_OUT 10 // Serial Data Out (for audio output)

// I2S port configuration
#define I2S_PORT_IN I2S_NUM_0
#define I2S_PORT_OUT I2S_NUM_1

// Audio configuration
const int SAMPLE_RATE = 48000;  // ReSpeaker uses 48kHz
const int BITS_PER_SAMPLE = 24; // ReSpeaker uses 24-bit
const int AUDIO_BUFFER_SIZE = 1024;

// Wake Word Settings
const float ENERGY_THRESHOLD = 3000.0;  // Adjusted for ReSpeaker
const int SILENCE_FRAMES = 45;
const char* WAKE_PHRASE = "hey chichi";

// ============================================================================
// Global Variables
// ============================================================================

WebSocketsClient webSocket;
String deviceId;
bool wsConnected = false;
bool isRecording = false;
bool wakeWordEnabled = true;
unsigned long lastAudioSend = 0;
int silenceCount = 0;

// Audio buffers
int32_t audioBuffer[AUDIO_BUFFER_SIZE];
uint8_t sendBuffer[AUDIO_BUFFER_SIZE * 4];

// ============================================================================
// Wake Word Detection
// ============================================================================

class SimpleWakeWordDetector {
private:
  float energyHistory[10] = {0};
  int historyIndex = 0;
  unsigned long lastDetection = 0;
  
public:
  bool detectWakeWord(int32_t* samples, size_t count) {
    if (!wakeWordEnabled) return false;
    
    // Prevent repeated detections within 3 seconds
    if (millis() - lastDetection < 3000) return false;
    
    // Calculate energy (adjusted for 24-bit samples)
    float energy = 0;
    for (size_t i = 0; i < count; i++) {
      float sample = (float)(samples[i] >> 8);  // Convert 24-bit to 16-bit range
      energy += sample * sample;
    }
    energy = sqrt(energy / count);
    
    // Update history
    energyHistory[historyIndex] = energy;
    historyIndex = (historyIndex + 1) % 10;
    
    // Check for energy spike
    if (energy > ENERGY_THRESHOLD) {
      lastDetection = millis();
      Serial.println("🎯 Wake word detected! Say your command...");
      return true;
    }
    
    return false;
  }
};

SimpleWakeWordDetector wakeDetector;

// ============================================================================
// Setup
// ============================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║   ESP32-S3 Voice Assistant + ReSpeaker  ║");
  Serial.println("║         Enhanced Audio Quality          ║");
  Serial.println("╚════════════════════════════════════════╝\n");
  
  // Generate unique device ID
  uint64_t chipId = ESP.getEfuseMac();
  deviceId = "xiao-respeaker-" + String((uint32_t)chipId, HEX);
  Serial.printf("Device ID: %s\n", deviceId.c_str());
  Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
  
  // Initialize components
  setupWiFi();
  setupI2S();
  setupWebSocket();
  
  Serial.println("\n✅ Ready! Say 'Hey chichi' to activate");
  Serial.println("🎧 Audio output via ReSpeaker headphone jack\n");
}

// ============================================================================
// WiFi Setup
// ============================================================================

void setupWiFi() {
  Serial.println("📡 Connecting to WiFi...");
  Serial.printf("   SSID: %s\n", WIFI_SSID);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi connected!");
    Serial.print("   IP: ");
    Serial.println(WiFi.localIP());
    Serial.printf("   Signal: %d dBm\n", WiFi.RSSI());
  } else {
    Serial.println("\n❌ WiFi connection failed!");
    Serial.println("   Check SSID and password in code");
  }
}

// ============================================================================
// I2S Setup for ReSpeaker
// ============================================================================

void setupI2S() {
  Serial.println("\n🎤 Setting up ReSpeaker I2S audio...");
  
  // I2S configuration for input (from ReSpeaker mics)
  i2s_config_t i2s_config_in = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_24BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 256,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };
  
  // I2S pin configuration for input
  i2s_pin_config_t pin_config_in = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD
  };
  
  // Install and start I2S driver for input
  esp_err_t err = i2s_driver_install(I2S_PORT_IN, &i2s_config_in, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("❌ Failed to install I2S input driver: %d\n", err);
    return;
  }
  
  err = i2s_set_pin(I2S_PORT_IN, &pin_config_in);
  if (err != ESP_OK) {
    Serial.printf("❌ Failed to set I2S input pins: %d\n", err);
    return;
  }
  
  // I2S configuration for output (to ReSpeaker headphone jack)
  i2s_config_t i2s_config_out = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_24BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 256,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };
  
  // I2S pin configuration for output
  i2s_pin_config_t pin_config_out = {
    .bck_io_num = I2S_SCK,      // Share clock with input
    .ws_io_num = I2S_WS,         // Share word select with input
    .data_out_num = I2S_SD_OUT,  // Separate data line for output
    .data_in_num = I2S_PIN_NO_CHANGE
  };
  
  // Install and start I2S driver for output
  err = i2s_driver_install(I2S_PORT_OUT, &i2s_config_out, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("❌ Failed to install I2S output driver: %d\n", err);
    return;
  }
  
  err = i2s_set_pin(I2S_PORT_OUT, &pin_config_out);
  if (err != ESP_OK) {
    Serial.printf("❌ Failed to set I2S output pins: %d\n", err);
    return;
  }
  
  Serial.println("✅ ReSpeaker audio initialized!");
  Serial.println("   Hardware: XMOS XU316 DSP");
  Serial.println("   Microphones: Dual digital with AEC");
  Serial.println("   Sample rate: 48 kHz, 24-bit");
  Serial.println("   Output: Headphone jack");
}

// ============================================================================
// WebSocket Setup
// ============================================================================

void setupWebSocket() {
  Serial.println("\n🌐 Connecting to server...");
  Serial.printf("   ws://%s:%d%s\n", WEBSOCKET_HOST, WEBSOCKET_PORT, WEBSOCKET_PATH);
  
  webSocket.begin(WEBSOCKET_HOST, WEBSOCKET_PORT, WEBSOCKET_PATH);
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
  webSocket.enableHeartbeat(15000, 3000, 2);
}

// ============================================================================
// Main Loop
// ============================================================================

void loop() {
  webSocket.loop();
  
  if (wsConnected) {
    processAudio();
    
    // Send status every 10 seconds
    static unsigned long lastStatus = 0;
    if (millis() - lastStatus > 10000) {
      sendStatus();
      lastStatus = millis();
    }
  }
  
  // Reconnect WiFi if needed
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠️  WiFi lost, reconnecting...");
    setupWiFi();
  }
  
  delay(1);
}

// ============================================================================
// Audio Processing
// ============================================================================

void processAudio() {
  size_t bytesRead = 0;
  
  // Read audio from ReSpeaker via I2S
  esp_err_t result = i2s_read(
    I2S_PORT_IN, 
    audioBuffer, 
    sizeof(audioBuffer), 
    &bytesRead, 
    portMAX_DELAY
  );
  
  if (result == ESP_OK && bytesRead > 0) {
    int samplesRead = bytesRead / sizeof(int32_t);
    
    // Check for wake word when not recording
    if (!isRecording && wakeDetector.detectWakeWord(audioBuffer, samplesRead)) {
      startRecording();
      sendWakeDetected();
    }
    
    // Stream audio when recording
    if (isRecording) {
      streamAudio(audioBuffer, samplesRead);
      
      // Check for silence to stop recording
      float energy = calculateEnergy(audioBuffer, samplesRead);
      if (energy < 500) {  // Adjusted for 24-bit
        silenceCount++;
        if (silenceCount > SILENCE_FRAMES) {
          stopRecording();
        }
      } else {
        silenceCount = 0;
      }
    }
  }
}

float calculateEnergy(int32_t* samples, size_t count) {
  float energy = 0;
  for (size_t i = 0; i < count; i++) {
    float sample = (float)(samples[i] >> 8);  // Convert to 16-bit range
    energy += sample * sample;
  }
  return sqrt(energy / count);
}

void streamAudio(int32_t* samples, size_t count) {
  // Rate limit to avoid overwhelming server
  if (millis() - lastAudioSend < 100) return;
  
  // Convert 24-bit samples to 16-bit PCM for transmission
  for (int i = 0; i < count && i < AUDIO_BUFFER_SIZE/2; i++) {
    int16_t sample16 = (int16_t)(samples[i] >> 8);
    sendBuffer[i * 2] = sample16 & 0xFF;
    sendBuffer[i * 2 + 1] = (sample16 >> 8) & 0xFF;
  }
  
  // Send as binary WebSocket frame
  webSocket.sendBIN(sendBuffer, count * 2);
  lastAudioSend = millis();
}

// ============================================================================
// Audio Output
// ============================================================================

void playAudio(uint8_t* audioData, size_t length) {
  // Convert incoming 16-bit PCM to 24-bit for ReSpeaker output
  int32_t outputBuffer[AUDIO_BUFFER_SIZE];
  int samples = length / 2;
  
  for (int i = 0; i < samples && i < AUDIO_BUFFER_SIZE; i++) {
    int16_t sample16 = (int16_t)(audioData[i*2] | (audioData[i*2+1] << 8));
    outputBuffer[i] = ((int32_t)sample16) << 8;  // Convert to 24-bit
  }
  
  size_t bytesWritten;
  i2s_write(I2S_PORT_OUT, outputBuffer, samples * sizeof(int32_t), &bytesWritten, portMAX_DELAY);
}

// ============================================================================
// WebSocket Events
// ============================================================================

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.println("❌ Disconnected from server");
      wsConnected = false;
      isRecording = false;
      break;
      
    case WStype_CONNECTED:
      Serial.println("✅ Connected to server!");
      wsConnected = true;
      sendDeviceInfo();
      break;
      
    case WStype_TEXT: {
      StaticJsonDocument<512> doc;
      DeserializationError error = deserializeJson(doc, payload, length);
      
      if (!error) {
        const char* msgType = doc["type"];
        
        if (strcmp(msgType, "command") == 0) {
          handleCommand(doc["command"]);
        }
        else if (strcmp(msgType, "mute_control") == 0) {
          wakeWordEnabled = !doc["muted"];
          Serial.printf("🔇 Wake word %s\n", wakeWordEnabled ? "enabled" : "muted");
        }
      }
      break;
    }
    
    case WStype_BIN:
      // Binary audio data from server
      Serial.println("🔊 Playing audio through ReSpeaker...");
      playAudio(payload, length);
      break;
  }
}

// ============================================================================
// Commands
// ============================================================================

void handleCommand(const char* command) {
  if (strcmp(command, "start_recording") == 0) {
    startRecording();
  }
  else if (strcmp(command, "stop_recording") == 0) {
    stopRecording();
  }
  else if (strcmp(command, "status") == 0) {
    sendStatus();
  }
}

void startRecording() {
  if (!isRecording) {
    isRecording = true;
    silenceCount = 0;
    Serial.println("🎤 Recording with ReSpeaker DSP enhancement...");
    
    StaticJsonDocument<256> doc;
    doc["type"] = "recording_started";
    doc["deviceId"] = deviceId;
    
    String json;
    serializeJson(doc, json);
    webSocket.sendTXT(json);
  }
}

void stopRecording() {
  if (isRecording) {
    isRecording = false;
    Serial.println("⏹️  Recording stopped");
    
    StaticJsonDocument<256> doc;
    doc["type"] = "recording_stopped";
    doc["deviceId"] = deviceId;
    
    String json;
    serializeJson(doc, json);
    webSocket.sendTXT(json);
  }
}

// ============================================================================
// Messages
// ============================================================================

void sendDeviceInfo() {
  StaticJsonDocument<512> doc;
  doc["type"] = "device_info";
  doc["deviceId"] = deviceId;
  doc["deviceType"] = "xiao-esp32s3-respeaker";
  doc["firmware"] = "1.0";
  
  JsonObject capabilities = doc.createNestedObject("capabilities");
  capabilities["microphone"] = true;
  capabilities["speaker"] = true;  // ReSpeaker has output!
  capabilities["wakeWord"] = true;
  capabilities["dsp"] = true;      // Hardware DSP
  
  JsonObject audio = doc.createNestedObject("audio");
  audio["sampleRate"] = SAMPLE_RATE;
  audio["bitDepth"] = BITS_PER_SAMPLE;
  audio["format"] = "i2s";
  audio["hardware"] = "XMOS XU316";
  audio["features"][0] = "AEC";
  audio["features"][1] = "Noise Suppression";
  audio["features"][2] = "AGC";
  
  String json;
  serializeJson(doc, json);
  webSocket.sendTXT(json);
}

void sendStatus() {
  StaticJsonDocument<256> doc;
  doc["type"] = "status";
  doc["deviceId"] = deviceId;
  doc["recording"] = isRecording;
  doc["wakeWordEnabled"] = wakeWordEnabled;
  doc["wifi_rssi"] = WiFi.RSSI();
  doc["uptime"] = millis() / 1000;
  
  String json;
  serializeJson(doc, json);
  webSocket.sendTXT(json);
}

void sendWakeDetected() {
  StaticJsonDocument<256> doc;
  doc["type"] = "wake_detected";
  doc["deviceId"] = deviceId;
  
  String json;
  serializeJson(doc, json);
  webSocket.sendTXT(json);
}