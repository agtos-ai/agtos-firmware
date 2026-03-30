/**
 * Audio Streaming for ESP32 Voice Device
 * Handles audio capture, buffering, and streaming
 */

#ifndef AUDIO_STREAM_H
#define AUDIO_STREAM_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/i2s.h"

#ifdef __cplusplus
extern "C" {
#endif

// Audio configuration
#define AUDIO_SAMPLE_RATE       16000
#define AUDIO_BITS_PER_SAMPLE   16
#define AUDIO_CHANNELS          1
#define AUDIO_BUFFER_SIZE       (1024 * 2)  // 2KB buffer
#define AUDIO_CHUNK_SIZE        640         // 20ms at 16kHz
#define AUDIO_RING_BUFFER_SIZE  (16 * 1024) // 16KB ring buffer

// Audio format
typedef enum {
    AUDIO_FORMAT_PCM16,
    AUDIO_FORMAT_PCM8,
    AUDIO_FORMAT_OPUS,
    AUDIO_FORMAT_G711
} audio_format_t;

// Audio stream configuration
typedef struct {
    uint32_t sample_rate;
    uint8_t bits_per_sample;
    uint8_t channels;
    audio_format_t format;
    bool echo_cancellation;
    bool noise_suppression;
    bool auto_gain_control;
} audio_config_t;

// Audio chunk for streaming
typedef struct {
    uint8_t *data;
    size_t size;
    uint32_t timestamp;
    uint32_t sequence_num;
} audio_chunk_t;

// Ring buffer for audio
typedef struct {
    uint8_t *buffer;
    size_t size;
    size_t head;
    size_t tail;
    size_t available;
    SemaphoreHandle_t mutex;
} audio_ring_buffer_t;

/**
 * Initialize audio streaming
 * @param config Audio configuration
 * @return ESP_OK on success
 */
esp_err_t audio_stream_init(const audio_config_t *config);

/**
 * Start audio capture
 * @return ESP_OK on success
 */
esp_err_t audio_stream_start(void);

/**
 * Stop audio capture
 * @return ESP_OK on success
 */
esp_err_t audio_stream_stop(void);

/**
 * Read audio chunk from stream
 * @param chunk Output audio chunk
 * @param timeout_ms Timeout in milliseconds
 * @return ESP_OK on success, ESP_ERR_TIMEOUT on timeout
 */
esp_err_t audio_stream_read(audio_chunk_t *chunk, uint32_t timeout_ms);

/**
 * Write audio to ring buffer
 * @param data Audio data
 * @param size Data size in bytes
 * @return Number of bytes written
 */
size_t audio_buffer_write(const uint8_t *data, size_t size);

/**
 * Read audio from ring buffer
 * @param data Output buffer
 * @param size Size to read
 * @return Number of bytes read
 */
size_t audio_buffer_read(uint8_t *data, size_t size);

/**
 * Get available bytes in ring buffer
 * @return Available bytes
 */
size_t audio_buffer_available(void);

/**
 * Clear audio buffer
 */
void audio_buffer_clear(void);

/**
 * Apply audio processing (VAD, AGC, etc.)
 * @param input Input audio buffer
 * @param output Output audio buffer
 * @param size Buffer size
 * @return ESP_OK on success
 */
esp_err_t audio_process(const int16_t *input, int16_t *output, size_t size);

/**
 * Encode audio chunk
 * @param pcm_data Raw PCM data
 * @param pcm_size PCM data size
 * @param encoded_data Output encoded data
 * @param encoded_size Output encoded size
 * @param format Target format
 * @return ESP_OK on success
 */
esp_err_t audio_encode(const uint8_t *pcm_data, size_t pcm_size,
                       uint8_t *encoded_data, size_t *encoded_size,
                       audio_format_t format);

/**
 * Calculate RMS energy of audio
 * @param samples Audio samples
 * @param num_samples Number of samples
 * @return RMS energy
 */
float audio_calculate_rms(const int16_t *samples, size_t num_samples);

#ifdef __cplusplus
}
#endif

#endif // AUDIO_STREAM_H