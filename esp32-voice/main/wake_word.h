/**
 * Wake Word Detection for ESP32 Voice Device
 * Lightweight wake word detection using energy-based VAD and pattern matching
 */

#ifndef WAKE_WORD_H
#define WAKE_WORD_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#ifdef __cplusplus
extern "C" {
#endif

// Wake word configuration
#define WAKE_WORD_BUFFER_SIZE   (16000 * 2)  // 2 seconds at 16kHz
#define WAKE_WORD_THRESHOLD     500.0f       // Energy threshold
#define VAD_THRESHOLD          100.0f        // Voice activity detection threshold
#define WAKE_CONFIDENCE_MIN    0.7f          // Minimum confidence for wake word

// Wake word detection states
typedef enum {
    WAKE_STATE_IDLE,
    WAKE_STATE_LISTENING,
    WAKE_STATE_DETECTED,
    WAKE_STATE_PROCESSING
} wake_state_t;

// Wake word detection result
typedef struct {
    bool detected;
    float confidence;
    uint32_t timestamp;
    int16_t *audio_buffer;
    size_t buffer_size;
} wake_detection_t;

/**
 * Initialize wake word detection
 * @param wake_word The wake word to detect (e.g., "hey_chichi")
 * @return ESP_OK on success
 */
esp_err_t wake_word_init(const char *wake_word);

/**
 * Process audio buffer for wake word detection
 * @param audio_data Raw audio samples (16-bit PCM)
 * @param data_size Size of audio data in bytes
 * @param result Output detection result
 * @return ESP_OK on success
 */
esp_err_t wake_word_process(const int16_t *audio_data, size_t data_size, 
                           wake_detection_t *result);

/**
 * Wake word detection task
 * Continuously monitors audio input for wake word
 * @param pvParameters Event group for signaling wake detection
 */
void wake_word_task(void *pvParameters);

/**
 * Calculate audio energy for VAD
 * @param audio_buffer Audio samples
 * @param num_samples Number of samples
 * @return Energy value
 */
float calculate_audio_energy(const uint8_t *audio_buffer, size_t num_samples);

/**
 * Reset wake word detector
 */
void wake_word_reset(void);

/**
 * Get current wake word state
 * @return Current state
 */
wake_state_t wake_word_get_state(void);

/**
 * Set wake word sensitivity
 * @param sensitivity Value from 0.0 (least sensitive) to 1.0 (most sensitive)
 */
void wake_word_set_sensitivity(float sensitivity);

#ifdef __cplusplus
}
#endif

#endif // WAKE_WORD_H