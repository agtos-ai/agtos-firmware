/**
 * ESP32 Voice Device Firmware
 * Main entry point and task coordination
 * Following agtos.firmware.esp32.v1 specification
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_ota_ops.h"
#include "driver/i2s.h"
#include "driver/gpio.h"

#include "wifi_manager.h"
#include "wake_word.h"
#include "audio_stream.h"
#include "websocket_client.h"
#include "device_mgmt.h"

// ============================================================================
// CONFIGURATION
// ============================================================================

static const char *TAG = "AGTOS_VOICE";

// Pin definitions for INMP441 microphone
#define I2S_WS_PIN      15  // Word Select (L/R)
#define I2S_SD_PIN      32  // Serial Data
#define I2S_SCK_PIN     14  // Serial Clock

// LED indicators
#define LED_WIFI_PIN    2   // Blue - WiFi status
#define LED_WAKE_PIN    4   // Green - Wake word detected
#define LED_ACTIVE_PIN  5   // Red - Recording active

// Button
#define BUTTON_PIN      0   // Boot button for manual trigger

// Event group bits
#define WIFI_CONNECTED_BIT  BIT0
#define WAKE_DETECTED_BIT   BIT1
#define STREAM_ACTIVE_BIT   BIT2
#define OTA_AVAILABLE_BIT   BIT3

// Task priorities
#define WAKE_WORD_TASK_PRIORITY    5
#define AUDIO_STREAM_TASK_PRIORITY 4
#define WEBSOCKET_TASK_PRIORITY    3
#define LED_TASK_PRIORITY          1

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

static EventGroupHandle_t s_event_group;
static SemaphoreHandle_t s_audio_mutex;
static bool s_is_streaming = false;
static websocket_handle_t s_websocket = NULL;

// ============================================================================
// LED CONTROL
// ============================================================================

static void init_leds(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED_WIFI_PIN) | 
                       (1ULL << LED_WAKE_PIN) | 
                       (1ULL << LED_ACTIVE_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    
    // Turn off all LEDs
    gpio_set_level(LED_WIFI_PIN, 0);
    gpio_set_level(LED_WAKE_PIN, 0);
    gpio_set_level(LED_ACTIVE_PIN, 0);
}

static void led_task(void *pvParameters) {
    EventBits_t bits;
    bool wifi_blink = false;
    
    while (1) {
        bits = xEventGroupGetBits(s_event_group);
        
        // WiFi LED - solid when connected, blink when connecting
        if (bits & WIFI_CONNECTED_BIT) {
            gpio_set_level(LED_WIFI_PIN, 1);
        } else {
            gpio_set_level(LED_WIFI_PIN, wifi_blink ? 1 : 0);
            wifi_blink = !wifi_blink;
        }
        
        // Wake LED - on when wake word detected
        gpio_set_level(LED_WAKE_PIN, (bits & WAKE_DETECTED_BIT) ? 1 : 0);
        
        // Active LED - on when streaming
        gpio_set_level(LED_ACTIVE_PIN, (bits & STREAM_ACTIVE_BIT) ? 1 : 0);
        
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// ============================================================================
// BUTTON HANDLING
// ============================================================================

static void IRAM_ATTR button_isr_handler(void *arg) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xEventGroupSetBitsFromISR(s_event_group, WAKE_DETECTED_BIT, 
                              &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

static void init_button(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUTTON_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE
    };
    gpio_config(&io_conf);
    
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_PIN, button_isr_handler, NULL);
}

// ============================================================================
// I2S AUDIO CONFIGURATION
// ============================================================================

static void init_i2s(void) {
    i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_RX,
        .sample_rate = 16000,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 1024,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };
    
    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_SCK_PIN,
        .ws_io_num = I2S_WS_PIN,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_SD_PIN
    };
    
    ESP_ERROR_CHECK(i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL));
    ESP_ERROR_CHECK(i2s_set_pin(I2S_NUM_0, &pin_config));
    ESP_ERROR_CHECK(i2s_zero_dma_buffer(I2S_NUM_0));
    
    ESP_LOGI(TAG, "I2S audio input initialized");
}

// ============================================================================
// MAIN AUDIO PROCESSING TASK
// ============================================================================

static void audio_processing_task(void *pvParameters) {
    EventBits_t bits;
    uint8_t *audio_buffer = malloc(AUDIO_BUFFER_SIZE);
    size_t bytes_read;
    
    if (!audio_buffer) {
        ESP_LOGE(TAG, "Failed to allocate audio buffer");
        vTaskDelete(NULL);
        return;
    }
    
    while (1) {
        // Wait for wake word detection
        bits = xEventGroupWaitBits(s_event_group,
                                  WAKE_DETECTED_BIT,
                                  pdTRUE,  // Clear bits
                                  pdFALSE,
                                  portMAX_DELAY);
        
        if (bits & WAKE_DETECTED_BIT) {
            ESP_LOGI(TAG, "Wake word detected! Starting audio stream...");
            
            // Set streaming active
            xEventGroupSetBits(s_event_group, STREAM_ACTIVE_BIT);
            s_is_streaming = true;
            
            // Start WebSocket connection if not connected
            if (!websocket_is_connected(s_websocket)) {
                if (websocket_connect(s_websocket) != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to connect WebSocket");
                    xEventGroupClearBits(s_event_group, STREAM_ACTIVE_BIT);
                    s_is_streaming = false;
                    continue;
                }
            }
            
            // Stream audio until silence detected or timeout
            int silence_count = 0;
            const int max_silence = 50; // 50 * 20ms = 1 second
            
            while (s_is_streaming && silence_count < max_silence) {
                // Read audio from I2S
                esp_err_t ret = i2s_read(I2S_NUM_0, audio_buffer, 
                                        AUDIO_BUFFER_SIZE, &bytes_read, 
                                        pdMS_TO_TICKS(20));
                
                if (ret == ESP_OK && bytes_read > 0) {
                    // Simple VAD - check audio energy
                    float energy = calculate_audio_energy(audio_buffer, bytes_read);
                    
                    if (energy > VAD_THRESHOLD) {
                        silence_count = 0;
                        
                        // Send audio chunk via WebSocket
                        if (xSemaphoreTake(s_audio_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                            websocket_send_audio(s_websocket, audio_buffer, bytes_read);
                            xSemaphoreGive(s_audio_mutex);
                        }
                    } else {
                        silence_count++;
                    }
                }
                
                vTaskDelay(pdMS_TO_TICKS(10));
            }
            
            ESP_LOGI(TAG, "Audio stream ended (silence detected)");
            
            // Clear streaming active
            xEventGroupClearBits(s_event_group, STREAM_ACTIVE_BIT);
            s_is_streaming = false;
            
            // Send end-of-stream marker
            websocket_send_eos(s_websocket);
        }
    }
    
    free(audio_buffer);
    vTaskDelete(NULL);
}

// ============================================================================
// WIFI EVENT HANDLER
// ============================================================================

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(s_event_group, WIFI_CONNECTED_BIT);
        esp_wifi_connect();
        ESP_LOGI(TAG, "Reconnecting to WiFi...");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_event_group, WIFI_CONNECTED_BIT);
    }
}

// ============================================================================
// WEBSOCKET EVENT HANDLER
// ============================================================================

static void websocket_event_handler(websocket_event_t event, void *data, size_t len) {
    switch (event) {
        case WEBSOCKET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "WebSocket connected");
            // Send device identification
            device_send_identification(s_websocket);
            break;
            
        case WEBSOCKET_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "WebSocket disconnected");
            s_is_streaming = false;
            xEventGroupClearBits(s_event_group, STREAM_ACTIVE_BIT);
            break;
            
        case WEBSOCKET_EVENT_DATA:
            // Handle incoming data (TTS audio, commands, etc.)
            handle_websocket_data(data, len);
            break;
            
        case WEBSOCKET_EVENT_ERROR:
            ESP_LOGE(TAG, "WebSocket error");
            break;
    }
}

// ============================================================================
// MAIN APPLICATION
// ============================================================================

void app_main(void) {
    ESP_LOGI(TAG, "===========================================");
    ESP_LOGI(TAG, "agtOS ESP32 Voice Device v1.0");
    ESP_LOGI(TAG, "Device ID: %s", DEVICE_ID);
    ESP_LOGI(TAG, "===========================================");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Create event group and mutex
    s_event_group = xEventGroupCreate();
    s_audio_mutex = xSemaphoreCreateMutex();
    
    // Initialize components
    init_leds();
    init_button();
    init_i2s();
    
    // Initialize WiFi
    wifi_init(wifi_event_handler);
    
    // Initialize wake word detection
    wake_word_init(WAKE_WORD);
    
    // Initialize WebSocket client
    s_websocket = websocket_init(WEBRTC_SERVER, websocket_event_handler);
    
    // Initialize device management (certificates, OTA, etc.)
    device_mgmt_init();
    
    // Create tasks
    xTaskCreate(led_task, "led_task", 2048, NULL, 
                LED_TASK_PRIORITY, NULL);
    
    xTaskCreate(wake_word_task, "wake_word", 4096, s_event_group, 
                WAKE_WORD_TASK_PRIORITY, NULL);
    
    xTaskCreate(audio_processing_task, "audio_proc", 8192, NULL, 
                AUDIO_STREAM_TASK_PRIORITY, NULL);
    
    xTaskCreate(websocket_task, "websocket", 4096, s_websocket, 
                WEBSOCKET_TASK_PRIORITY, NULL);
    
    ESP_LOGI(TAG, "System initialized. Waiting for wake word: \"%s\"", WAKE_WORD);
    
    // Print heap info
    ESP_LOGI(TAG, "Free heap: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "Minimum free heap: %d bytes", esp_get_minimum_free_heap_size());
    
    // Main loop (could handle OTA updates, etc.)
    while (1) {
        EventBits_t bits = xEventGroupGetBits(s_event_group);
        
        // Check for OTA update
        if (bits & OTA_AVAILABLE_BIT) {
            ESP_LOGI(TAG, "OTA update available, starting update...");
            perform_ota_update();
            xEventGroupClearBits(s_event_group, OTA_AVAILABLE_BIT);
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}