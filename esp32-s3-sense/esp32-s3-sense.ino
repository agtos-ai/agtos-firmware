/**
 * ESP32-S3 Voice Assistant - Advanced Wake Word Version
 * For: Seeed Studio XIAO ESP32-S3 Sense
 * 
 * Features:
 * - Improved wake word detection with frequency analysis
 * - Server-side wake word verification
 * - Adaptive threshold based on ambient noise
 * - Voice activity detection (VAD)
 * 
 * Version: 2.0 - Production Ready
 */

#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ESP_I2S.h>
#include <ArduinoJson.h>

// ============================================================================
// Configuration
// ============================================================================

const char* WIFI_SSID = "Redux";
const char* WIFI_PASSWORD = "day-one-21";
const char* WEBSOCKET_HOST = "192.168.3.196";
const int WEBSOCKET_PORT = 8765;
const char* WEBSOCKET_PATH = "/ws/audio";

// ============================================================================
// Advanced Audio Configuration
// ============================================================================

const int SAMPLE_RATE = 16000;
const int AUDIO_BUFFER_SIZE = 1600;  // 100ms at 16kHz
const int PRE_WAKE_BUFFER_SIZE = 8000;  // 500ms pre-buffer for wake word verification

// Dynamic thresholds
float ENERGY_THRESHOLD = 12000.0;  // Will be adjusted based on ambient noise
const float MIN_ENERGY_THRESHOLD = 8000.0;
const float MAX_ENERGY_THRESHOLD = 20000.0;
const float VOICE_FREQ_MIN = 85.0;   // Human voice fundamental frequency range
const float VOICE_FREQ_MAX = 3000.0;

// Wake word timing
const int MIN_WAKE_DURATION_MS = 500;   // Minimum duration for wake phrase
const int MAX_WAKE_DURATION_MS = 2000;  // Maximum duration for wake phrase
const int POST_WAKE_RECORD_MS = 5000;   // Record 5 seconds after wake word

// ============================================================================
// Global Variables
// ============================================================================

WebSocketsClient webSocket;
I2SClass I2S;

String deviceId;
bool wsConnected = false;
bool isRecording = false;
bool wakeWordEnabled = true;
bool muted = false;
unsigned long lastStatusUpdate = 0;
const unsigned long STATUS_UPDATE_INTERVAL = 10000; // Send status every 10 seconds
bool serverVerificationMode = false;
unsigned long recordingStartTime = 0;
unsigned long lastAudioSend = 0;

// Reconnection backoff
unsigned long disconnectTime = 0;
unsigned long reconnectDelay = 1000; // Start with 1 second
const unsigned long MAX_RECONNECT_DELAY = 30000; // Max 30 seconds
bool shouldReconnect = false;

// Audio buffers
int16_t micBuffer[AUDIO_BUFFER_SIZE];
int16_t preWakeBuffer[PRE_WAKE_BUFFER_SIZE];  // Circular buffer for pre-wake audio
int preWakeIndex = 0;
uint8_t audioBuffer[AUDIO_BUFFER_SIZE * 2];

// Ambient noise calibration
float ambientNoiseLevel = 0;
int calibrationSamples = 0;
const int CALIBRATION_SAMPLES_NEEDED = 50;

// ============================================================================
// Advanced Wake Word Detector
// ============================================================================

class AdvancedWakeWordDetector {
private:
  // Energy tracking
  float energyHistory[20] = {0};
  int historyIndex = 0;
  float avgEnergy = 0;
  
  // Frequency tracking
  float dominantFreqHistory[10] = {0};
  int freqHistoryIndex = 0;
  
  // Detection state
  unsigned long lastDetection = 0;
  unsigned long potentialWakeStart = 0;
  bool inPotentialWake = false;
  
  // Voice activity detection
  int voiceFrameCount = 0;
  int silenceFrameCount = 0;
  
public:
  
  float calculateDominantFrequency(int16_t* samples, size_t count) {
    // Simple autocorrelation for pitch detection
    const int MIN_PERIOD = SAMPLE_RATE / VOICE_FREQ_MAX;  // ~5 samples at 16kHz for 3000Hz
    const int MAX_PERIOD = SAMPLE_RATE / VOICE_FREQ_MIN;  // ~188 samples at 16kHz for 85Hz
    
    float maxCorr = 0;
    int bestPeriod = 0;
    
    for (int period = MIN_PERIOD; period < MAX_PERIOD && period < count/2; period++) {
      float corr = 0;
      for (int i = 0; i < count - period; i++) {
        corr += (float)samples[i] * samples[i + period];
      }
      
      if (corr > maxCorr) {
        maxCorr = corr;
        bestPeriod = period;
      }
    }
    
    if (bestPeriod > 0) {
      return (float)SAMPLE_RATE / bestPeriod;
    }
    return 0;
  }
  
  bool isVoiceLike(float energy, float frequency) {
    // Check if audio characteristics match human voice
    return (energy > ENERGY_THRESHOLD * 0.5 &&  // At least half threshold
            frequency > VOICE_FREQ_MIN && 
            frequency < VOICE_FREQ_MAX);
  }
  
  bool detectWakeWord(int16_t* samples, size_t count) {
    if (!wakeWordEnabled) return false;
    
    // Prevent repeated detections within 5 seconds
    if (millis() - lastDetection < 5000) return false;
    
    // Calculate energy
    float energy = 0;
    for (size_t i = 0; i < count; i++) {
      float sample = (float)samples[i];
      energy += sample * sample;
    }
    energy = sqrt(energy / count);
    
    // Calculate dominant frequency
    float dominantFreq = calculateDominantFrequency(samples, count);
    
    // Update histories
    energyHistory[historyIndex] = energy;
    historyIndex = (historyIndex + 1) % 20;
    
    dominantFreqHistory[freqHistoryIndex] = dominantFreq;
    freqHistoryIndex = (freqHistoryIndex + 1) % 10;
    
    // Calculate average energy for comparison
    avgEnergy = 0;
    for (int i = 0; i < 20; i++) {
      avgEnergy += energyHistory[i];
    }
    avgEnergy /= 20;
    
    // Voice activity detection
    if (isVoiceLike(energy, dominantFreq)) {
      voiceFrameCount++;
      silenceFrameCount = 0;
      
      // Check for sudden energy increase (wake word start)
      if (!inPotentialWake && energy > ENERGY_THRESHOLD && energy > avgEnergy * 2.5) {
        inPotentialWake = true;
        potentialWakeStart = millis();
        Serial.printf("🎤 Potential wake word start (Energy: %.0f, Freq: %.0fHz)\n", energy, dominantFreq);
      }
    } else {
      silenceFrameCount++;
      if (silenceFrameCount > 5) {
        voiceFrameCount = 0;
      }
    }
    
    // Check if we've been in potential wake for appropriate duration
    if (inPotentialWake) {
      unsigned long wakeDuration = millis() - potentialWakeStart;
      
      if (wakeDuration > MIN_WAKE_DURATION_MS && wakeDuration < MAX_WAKE_DURATION_MS) {
        if (voiceFrameCount > 5) {  // At least 5 voice frames detected
          // Wake word detected!
          lastDetection = millis();
          inPotentialWake = false;
          voiceFrameCount = 0;
          
          Serial.println("🎯 Wake word detected! Sending for verification...");
          return true;
        }
      } else if (wakeDuration > MAX_WAKE_DURATION_MS) {
        // Too long, reset
        inPotentialWake = false;
        voiceFrameCount = 0;
      }
    }
    
    return false;
  }
  
  void calibrateNoiseFloor(int16_t* samples, size_t count) {
    if (calibrationSamples < CALIBRATION_SAMPLES_NEEDED) {
      float energy = 0;
      for (size_t i = 0; i < count; i++) {
        float sample = (float)samples[i];
        energy += sample * sample;
      }
      energy = sqrt(energy / count);
      
      ambientNoiseLevel += energy;
      calibrationSamples++;
      
      if (calibrationSamples == CALIBRATION_SAMPLES_NEEDED) {
        ambientNoiseLevel /= CALIBRATION_SAMPLES_NEEDED;
        // Set threshold to 5x ambient noise level
        ENERGY_THRESHOLD = constrain(ambientNoiseLevel * 5, MIN_ENERGY_THRESHOLD, MAX_ENERGY_THRESHOLD);
        Serial.printf("📊 Calibration complete. Ambient: %.0f, Threshold: %.0f\n", 
                     ambientNoiseLevel, ENERGY_THRESHOLD);
      }
    }
  }
};

AdvancedWakeWordDetector wakeDetector;

// ============================================================================
// Setup Functions
// ============================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║  ESP32-S3 Voice Assistant Advanced v2  ║");
  Serial.println("║     With Smart Wake Word Detection     ║");
  Serial.println("╚════════════════════════════════════════╝\n");
  
  // Generate unique device ID
  uint64_t chipId = ESP.getEfuseMac();
  deviceId = "xiao-voice-" + String((uint32_t)chipId, HEX);
  Serial.printf("Device ID: %s\n", deviceId.c_str());
  
  setupWiFi();
  setupMicrophone();
  setupWebSocket();
  
  Serial.println("\n✅ Setup complete!");
  Serial.println("🎤 Calibrating ambient noise level...");
  Serial.println("📢 Please be quiet for 5 seconds...\n");
}

void setupWiFi() {
  Serial.println("📡 Connecting to WiFi...");
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
    Serial.printf("   IP: %s, Signal: %d dBm\n", 
                 WiFi.localIP().toString().c_str(), WiFi.RSSI());
  }
}

void setupMicrophone() {
  Serial.println("🎤 Initializing PDM microphone...");
  
  I2S.setPins(42, 41, -1, -1);  // PDM pins for XIAO ESP32-S3 Sense
  
  if (!I2S.begin(I2S_MODE_PDM_RX, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
    Serial.println("❌ Failed to initialize I2S!");
    while (1);
  }
  
  Serial.println("✅ Microphone initialized");
}

void setupWebSocket() {
  Serial.println("🌐 Initializing WebSocket...");
  
  webSocket.begin(WEBSOCKET_HOST, WEBSOCKET_PORT, WEBSOCKET_PATH);
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(3000);
  
  Serial.printf("   Connecting to: ws://%s:%d%s\n", 
               WEBSOCKET_HOST, WEBSOCKET_PORT, WEBSOCKET_PATH);
}

// ============================================================================
// WebSocket Event Handler
// ============================================================================

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      wsConnected = false;
      Serial.println("❌ WebSocket disconnected");
      // Set up reconnection with backoff
      disconnectTime = millis();
      shouldReconnect = true;
      Serial.printf("🔄 Will reconnect in %lu ms\n", reconnectDelay);
      break;
      
    case WStype_CONNECTED:
      wsConnected = true;
      Serial.println("✅ WebSocket connected");
      
      // Reset reconnection backoff on successful connection
      reconnectDelay = 1000;
      shouldReconnect = false;
      
      delay(500); // Give server time to set up handlers
      sendDeviceRegistration();
      sendStatusUpdate(); // Also send initial status
      break;
      
    case WStype_TEXT:
      handleServerMessage((char*)payload);
      break;
      
    case WStype_BIN:
      // Audio response from server
      Serial.printf("🔊 Received audio response: %d bytes\n", length);
      break;
  }
}

void handleServerMessage(const char* message) {
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, message);
  
  if (!error) {
    const char* type = doc["type"];
    
    if (strcmp(type, "wake_verification") == 0) {
      bool verified = doc["verified"];
      if (verified) {
        Serial.println("✅ Wake word verified by server!");
        startRecording();
      } else {
        Serial.println("❌ Wake word not verified by server");
      }
    }
    else if (strcmp(type, "transcription") == 0) {
      const char* text = doc["text"];
      Serial.printf("📝 Transcription: %s\n", text);
    }
    else if (strcmp(type, "response") == 0) {
      const char* text = doc["text"];
      Serial.printf("🤖 Response: %s\n", text);
    }
  }
}

void sendDeviceRegistration() {
  StaticJsonDocument<256> doc;
  doc["type"] = "device_register";
  doc["deviceId"] = deviceId;
  doc["capabilities"]["wake_word"] = true;
  doc["capabilities"]["pre_buffer"] = true;
  doc["firmware_version"] = "2.0";
  
  String json;
  serializeJson(doc, json);
  Serial.print("📤 Sending registration: ");
  Serial.println(json);
  webSocket.sendTXT(json);
  Serial.println("✅ Registration sent");
}

// ============================================================================
// Recording Functions
// ============================================================================

void startRecording() {
  isRecording = true;
  recordingStartTime = millis();
  Serial.println("🔴 Recording started...");
  
  // Send pre-wake buffer if we have it
  sendPreWakeBuffer();
}

void stopRecording() {
  isRecording = false;
  Serial.println("⏹️ Recording stopped");
  
  // Send end-of-audio marker
  StaticJsonDocument<128> doc;
  doc["type"] = "audio_end";
  String json;
  serializeJson(doc, json);
  webSocket.sendTXT(json);
}

void sendPreWakeBuffer() {
  // Send the circular pre-wake buffer to include wake word in transcription
  Serial.println("📤 Sending pre-wake buffer for verification...");
  
  // Start from current position in circular buffer
  for (int i = 0; i < PRE_WAKE_BUFFER_SIZE; i++) {
    int idx = (preWakeIndex + i) % PRE_WAKE_BUFFER_SIZE;
    audioBuffer[i * 2] = preWakeBuffer[idx] & 0xFF;
    audioBuffer[i * 2 + 1] = (preWakeBuffer[idx] >> 8) & 0xFF;
  }
  
  // Mark this as wake word audio for server verification
  StaticJsonDocument<128> doc;
  doc["type"] = "wake_audio";
  String json;
  serializeJson(doc, json);
  webSocket.sendTXT(json);
  
  // Send the audio
  webSocket.sendBIN(audioBuffer, PRE_WAKE_BUFFER_SIZE * 2);
}

// ============================================================================
// Main Loop
// ============================================================================

void loop() {
  // Handle reconnection with exponential backoff
  if (shouldReconnect && !wsConnected && (millis() - disconnectTime > reconnectDelay)) {
    Serial.println("🔄 Attempting to reconnect...");
    webSocket.begin(WEBSOCKET_SERVER, WEBSOCKET_PORT, "/ws/audio");
    shouldReconnect = false;
    // Exponential backoff: double the delay up to max
    reconnectDelay = min(reconnectDelay * 2, MAX_RECONNECT_DELAY);
  }
  
  webSocket.loop();
  
  // Send periodic status updates if connected
  if (wsConnected && (millis() - lastStatusUpdate > STATUS_UPDATE_INTERVAL)) {
    sendStatusUpdate();
    lastStatusUpdate = millis();
  }
  
  // Read audio from microphone
  size_t bytesRead = I2S.readBytes((char*)micBuffer, sizeof(micBuffer));
  
  if (bytesRead > 0) {
    int samplesRead = bytesRead / 2;
    
    // Calibration phase (don't apply AGC during calibration)
    if (calibrationSamples < CALIBRATION_SAMPLES_NEEDED) {
      wakeDetector.calibrateNoiseFloor(micBuffer, samplesRead);
    } else {
      // Process audio with AGC only after calibration
      processAudioAGC(micBuffer, samplesRead);
    }
    
    // Store in pre-wake circular buffer
    for (int i = 0; i < samplesRead && i < PRE_WAKE_BUFFER_SIZE; i++) {
      preWakeBuffer[preWakeIndex] = micBuffer[i];
      preWakeIndex = (preWakeIndex + 1) % PRE_WAKE_BUFFER_SIZE;
    }
    
    // Normal operation after calibration
    if (calibrationSamples >= CALIBRATION_SAMPLES_NEEDED) {
      // Check for wake word
      if (!isRecording && wakeDetector.detectWakeWord(micBuffer, samplesRead)) {
        if (serverVerificationMode) {
          // Send to server for verification
          sendPreWakeBuffer();
        } else {
          // Direct start (less accurate but faster)
          startRecording();
        }
      }
      
      // Stream audio if recording
      if (isRecording && wsConnected) {
        // Check recording timeout
        if (millis() - recordingStartTime > POST_WAKE_RECORD_MS) {
          stopRecording();
        } else {
          // Convert to bytes and send
          for (int i = 0; i < samplesRead; i++) {
            audioBuffer[i * 2] = micBuffer[i] & 0xFF;
            audioBuffer[i * 2 + 1] = (micBuffer[i] >> 8) & 0xFF;
          }
          webSocket.sendBIN(audioBuffer, samplesRead * 2);
        }
      }
    }
  }
}

// ============================================================================
// Audio Processing
// ============================================================================

void processAudioAGC(int16_t* samples, size_t count) {
  const float AGC_TARGET = 8000.0;
  const float AGC_MAX_GAIN = 10.0;
  const float AGC_MIN_GAIN = 0.5;
  
  // Calculate RMS
  float rms = 0;
  for (size_t i = 0; i < count; i++) {
    float sample = (float)samples[i];
    rms += sample * sample;
  }
  rms = sqrt(rms / count);
  
  // Calculate gain
  float gain = 1.0;
  if (rms > 100 && rms < AGC_TARGET) {
    gain = AGC_TARGET / rms;
    gain = constrain(gain, AGC_MIN_GAIN, AGC_MAX_GAIN);
  }
  
  // Apply gain with soft clipping
  if (gain != 1.0) {
    for (size_t i = 0; i < count; i++) {
      float sample = (float)samples[i] * gain;
      
      // Soft clipping using tanh
      if (abs(sample) > 28000) {
        sample = 28000 * tanh(sample / 28000);
      }
      
      samples[i] = (int16_t)constrain(sample, -32768, 32767);
    }
  }
}

void sendStatusUpdate() {
  if (!wsConnected) return;
  
  StaticJsonDocument<256> doc;
  doc["type"] = "status";
  doc["deviceId"] = deviceId;
  doc["recording"] = isRecording;
  doc["wakeWordEnabled"] = wakeWordEnabled;
  doc["threshold"] = ENERGY_THRESHOLD;
  doc["ambient"] = ambientNoiseLevel;
  doc["wifi_rssi"] = WiFi.RSSI();
  doc["uptime"] = millis() / 1000;
  
  String json;
  serializeJson(doc, json);
  webSocket.sendTXT(json);
}