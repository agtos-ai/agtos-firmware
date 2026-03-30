/**
 * Device Management for ESP32 Voice Device
 * Handles device identity, certificates, OTA updates, and configuration
 */

#ifndef DEVICE_MGMT_H
#define DEVICE_MGMT_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_ota_ops.h"
#include "esp_websocket_client.h"

#ifdef __cplusplus
extern "C" {
#endif

// Device identification
#define DEVICE_ID           CONFIG_DEVICE_ID
#define DEVICE_TYPE         "esp32-voice"
#define DEVICE_VERSION      "1.0.0"
#define HARDWARE_VERSION    "1.0"

// Device configuration keys (NVS)
#define NVS_NAMESPACE       "agtos"
#define NVS_KEY_DEVICE_ID   "device_id"
#define NVS_KEY_WAKE_WORD   "wake_word"
#define NVS_KEY_SERVER_URL  "server_url"
#define NVS_KEY_AUTH_TOKEN  "auth_token"
#define NVS_KEY_CERT        "device_cert"
#define NVS_KEY_KEY         "device_key"

// OTA configuration
#define OTA_URL_SIZE        256
#define OTA_BUFFER_SIZE     1024

// Device info structure
typedef struct {
    char device_id[64];
    char device_type[32];
    char firmware_version[32];
    char hardware_version[32];
    char mac_address[18];
    uint32_t uptime_seconds;
    uint32_t free_heap;
    uint32_t total_heap;
    int8_t wifi_rssi;
    bool ota_available;
    char ota_version[32];
} device_info_t;

// Device configuration
typedef struct {
    char wake_word[64];
    char server_url[256];
    char auth_token[256];
    bool auto_update;
    bool telemetry_enabled;
    uint32_t heartbeat_interval;
} device_config_t;

// Certificate info
typedef struct {
    char *cert_pem;
    char *key_pem;
    char *ca_cert_pem;
    bool valid;
    uint32_t expiry_timestamp;
} device_cert_t;

/**
 * Initialize device management
 * @return ESP_OK on success
 */
esp_err_t device_mgmt_init(void);

/**
 * Get device information
 * @param info Output device info structure
 * @return ESP_OK on success
 */
esp_err_t device_get_info(device_info_t *info);

/**
 * Send device identification to server
 * @param websocket WebSocket handle
 * @return ESP_OK on success
 */
esp_err_t device_send_identification(esp_websocket_client_handle_t websocket);

/**
 * Load device configuration from NVS
 * @param config Output configuration
 * @return ESP_OK on success
 */
esp_err_t device_load_config(device_config_t *config);

/**
 * Save device configuration to NVS
 * @param config Configuration to save
 * @return ESP_OK on success
 */
esp_err_t device_save_config(const device_config_t *config);

/**
 * Update device configuration value
 * @param key Configuration key
 * @param value New value
 * @return ESP_OK on success
 */
esp_err_t device_update_config(const char *key, const char *value);

/**
 * Load device certificates
 * @param cert Output certificate structure
 * @return ESP_OK on success
 */
esp_err_t device_load_certificates(device_cert_t *cert);

/**
 * Store device certificates
 * @param cert Certificate structure to store
 * @return ESP_OK on success
 */
esp_err_t device_store_certificates(const device_cert_t *cert);

/**
 * Check for OTA updates
 * @param update_url URL to check for updates
 * @param current_version Current firmware version
 * @param new_version Output buffer for new version (if available)
 * @return ESP_OK if update available, ESP_ERR_NOT_FOUND if not
 */
esp_err_t device_check_ota_update(const char *update_url, 
                                  const char *current_version,
                                  char *new_version);

/**
 * Perform OTA update
 * @return ESP_OK on success
 */
esp_err_t perform_ota_update(void);

/**
 * Start OTA update from URL
 * @param url OTA firmware URL
 * @return ESP_OK on success
 */
esp_err_t device_ota_from_url(const char *url);

/**
 * Factory reset device
 * Erases all configuration and certificates
 * @return ESP_OK on success
 */
esp_err_t device_factory_reset(void);

/**
 * Reboot device
 * @param delay_ms Delay before reboot in milliseconds
 */
void device_reboot(uint32_t delay_ms);

/**
 * Get device uptime
 * @return Uptime in seconds
 */
uint32_t device_get_uptime(void);

/**
 * Enable/disable telemetry
 * @param enable true to enable, false to disable
 */
void device_set_telemetry(bool enable);

/**
 * Send telemetry data
 * @param websocket WebSocket handle
 * @return ESP_OK on success
 */
esp_err_t device_send_telemetry(esp_websocket_client_handle_t websocket);

#ifdef __cplusplus
}
#endif

#endif // DEVICE_MGMT_H