/**
 * WiFi Manager for ESP32 Voice Device
 * Handles WiFi connection and reconnection
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_wifi.h"
#include "esp_event.h"

#ifdef __cplusplus
extern "C" {
#endif

// WiFi Configuration
#define WIFI_SSID       CONFIG_WIFI_SSID
#define WIFI_PASSWORD   CONFIG_WIFI_PASSWORD
#define WIFI_MAX_RETRY  10

// WiFi event handler callback
typedef void (*wifi_event_callback_t)(void *arg, esp_event_base_t event_base,
                                      int32_t event_id, void *event_data);

/**
 * Initialize WiFi in station mode
 * @param event_handler Custom event handler for WiFi events
 * @return ESP_OK on success
 */
esp_err_t wifi_init(wifi_event_callback_t event_handler);

/**
 * Start WiFi connection
 * @return ESP_OK on success
 */
esp_err_t wifi_connect(void);

/**
 * Disconnect WiFi
 * @return ESP_OK on success
 */
esp_err_t wifi_disconnect(void);

/**
 * Get current WiFi connection status
 * @return true if connected
 */
bool wifi_is_connected(void);

/**
 * Get current IP address as string
 * @param ip_str Buffer to store IP string (min 16 bytes)
 * @return ESP_OK on success
 */
esp_err_t wifi_get_ip_str(char *ip_str);

#ifdef __cplusplus
}
#endif

#endif // WIFI_MANAGER_H