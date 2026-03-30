/**
 * WebSocket Client for ESP32 Voice Device
 * Handles WebSocket connection to agtOS server
 */

#ifndef WEBSOCKET_CLIENT_H
#define WEBSOCKET_CLIENT_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_websocket_client.h"

#ifdef __cplusplus
extern "C" {
#endif

// WebSocket configuration
#define WS_BUFFER_SIZE          (4 * 1024)  // 4KB buffer
#define WS_RECONNECT_TIMEOUT    5000        // 5 seconds
#define WS_PING_INTERVAL        30000       // 30 seconds
#define WS_MAX_RECONNECT        10          // Max reconnection attempts

// WebSocket handle
typedef esp_websocket_client_handle_t websocket_handle_t;

// WebSocket events
typedef enum {
    WEBSOCKET_EVENT_CONNECTED,
    WEBSOCKET_EVENT_DISCONNECTED,
    WEBSOCKET_EVENT_DATA,
    WEBSOCKET_EVENT_ERROR
} websocket_event_t;

// WebSocket event callback
typedef void (*websocket_event_handler_t)(websocket_event_t event, 
                                         void *data, size_t len);

// WebSocket message types
typedef enum {
    WS_MSG_AUDIO_STREAM,
    WS_MSG_AUDIO_END,
    WS_MSG_COMMAND,
    WS_MSG_RESPONSE,
    WS_MSG_HEARTBEAT,
    WS_MSG_DEVICE_INFO,
    WS_MSG_ERROR
} ws_message_type_t;

// WebSocket message header
typedef struct {
    ws_message_type_t type;
    uint32_t sequence;
    uint32_t timestamp;
    uint16_t payload_size;
    uint8_t flags;
} ws_message_header_t;

/**
 * Initialize WebSocket client
 * @param url WebSocket server URL
 * @param event_handler Event handler callback
 * @return WebSocket handle or NULL on failure
 */
websocket_handle_t websocket_init(const char *url, 
                                  websocket_event_handler_t event_handler);

/**
 * Connect to WebSocket server
 * @param handle WebSocket handle
 * @return ESP_OK on success
 */
esp_err_t websocket_connect(websocket_handle_t handle);

/**
 * Disconnect from WebSocket server
 * @param handle WebSocket handle
 * @return ESP_OK on success
 */
esp_err_t websocket_disconnect(websocket_handle_t handle);

/**
 * Check if WebSocket is connected
 * @param handle WebSocket handle
 * @return true if connected
 */
bool websocket_is_connected(websocket_handle_t handle);

/**
 * Send audio data via WebSocket
 * @param handle WebSocket handle
 * @param audio_data Audio data buffer
 * @param data_size Data size in bytes
 * @return ESP_OK on success
 */
esp_err_t websocket_send_audio(websocket_handle_t handle,
                               const uint8_t *audio_data, size_t data_size);

/**
 * Send end-of-stream marker
 * @param handle WebSocket handle
 * @return ESP_OK on success
 */
esp_err_t websocket_send_eos(websocket_handle_t handle);

/**
 * Send command/text message
 * @param handle WebSocket handle
 * @param command Command string
 * @return ESP_OK on success
 */
esp_err_t websocket_send_command(websocket_handle_t handle, const char *command);

/**
 * Send device information
 * @param handle WebSocket handle
 * @return ESP_OK on success
 */
esp_err_t websocket_send_device_info(websocket_handle_t handle);

/**
 * WebSocket task for handling incoming messages
 * @param pvParameters WebSocket handle
 */
void websocket_task(void *pvParameters);

/**
 * Handle incoming WebSocket data
 * @param data Data buffer
 * @param len Data length
 */
void handle_websocket_data(void *data, size_t len);

/**
 * Set reconnection parameters
 * @param handle WebSocket handle
 * @param enabled Enable auto-reconnect
 * @param interval Reconnect interval in ms
 * @param max_attempts Max reconnection attempts
 */
void websocket_set_reconnect(websocket_handle_t handle, bool enabled,
                            uint32_t interval, uint32_t max_attempts);

/**
 * Get WebSocket statistics
 * @param handle WebSocket handle
 * @param bytes_sent Output bytes sent
 * @param bytes_received Output bytes received
 * @param messages_sent Output messages sent
 * @param messages_received Output messages received
 */
void websocket_get_stats(websocket_handle_t handle,
                        uint64_t *bytes_sent, uint64_t *bytes_received,
                        uint32_t *messages_sent, uint32_t *messages_received);

#ifdef __cplusplus
}
#endif

#endif // WEBSOCKET_CLIENT_H