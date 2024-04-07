#include "main.h"
#include "hw_camera.h"
#include <smart_refrigerator_inferencing.h>
#include "edge-impulse-sdk/dsp/image/image.hpp"

#include <task.h>
#include <queue.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>

// Constants
#define TAG     "main"
#define BTN_PIN 0

#define EI_CAMERA_RAW_FRAME_BUFFER_COLS 240
#define EI_CAMERA_RAW_FRAME_BUFFER_ROWS 240
#define EI_CAMERA_FRAME_BYTE_SIZE       3
#define BMP_BUF_SIZE                    (EI_CAMERA_RAW_FRAME_BUFFER_COLS * EI_CAMERA_RAW_FRAME_BUFFER_ROWS * EI_CAMERA_FRAME_BYTE_SIZE)

#define MQTT_BROKER       "broker.hivemq.com"
#define MQTT_PORT         1883
#define HIVEMQ_USERNAME   "taist_aiot_05"
#define MQTT_HB_TOPIC     "taist/aiot/objdetection/dev_05"

#define WIFI_SSID         "Peet"
#define WIFI_PASSWORD     "peet3210"

// Global variables
WiFiClient wifi_client;
PubSubClient mqtt_client(wifi_client);

// Queue handle
QueueHandle_t evt_queue;

// Static variables
static uint8_t *bmp_buf;

// Static function prototypes
void print_memory(void);
void ei_prepare_feature(uint8_t *img_buf, signal_t *signal);
int ei_get_feature_callback(size_t offset, size_t length, float *out_ptr);
int32_t ei_use_result(ei_impulse_result_t result);
void EI_objDetection(void *pvParameter);
void comm_task(void *pvParameter);

void EI_objDetection(void *pvParameter) {
    while (true) {
        static uint32_t prev_millis = 0;
        bool detection;

        if (millis() - prev_millis > 500) {
            uint32_t Tstart, elapsed_time;
            uint32_t width, height;

            prev_millis = millis();
            Tstart = millis();

            // Get raw data
            ESP_LOGI(TAG, "Taking snapshot...");
            hw_camera_raw_snapshot(bmp_buf, &width, &height);
            elapsed_time = millis() - Tstart;
            ESP_LOGI(TAG, "Snapshot taken (%d) width: %d, height: %d", elapsed_time, width, height);
            print_memory();

            // Prepare feature
            Tstart = millis();
            ei::signal_t signal;
            ei_prepare_feature(bmp_buf, &signal);
            elapsed_time = millis() - Tstart;
            ESP_LOGI(TAG, "Feature taken (%d)", elapsed_time);
            print_memory();

            // Run classifier
            Tstart = millis();
            ei_impulse_result_t result = {0};
            bool debug_nn = false;
            run_classifier(&signal, &result, debug_nn);
            elapsed_time = millis() - Tstart;
            ESP_LOGI(TAG, "Classification done (%d)", elapsed_time);
            print_memory();

            // Use result
            ei_use_result(result);
            xQueueSend(evt_queue, &result, portMAX_DELAY);
        }
        delay(1000);
    }
}

void comm_task(void *pvParameter) {
    static uint32_t prev_ms = 0; // Last ms of sending MQTT

    // Initialize serial and network
    Serial.begin(115200);
    WiFi.mode(WIFI_OFF);
    delay(100);
    WiFi.mode(WIFI_STA);
    delay(100);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }

    mqtt_client.setServer(MQTT_BROKER, MQTT_PORT);
    mqtt_client.connect(HIVEMQ_USERNAME);

    while (true) {
        // Wait for event
        ei_impulse_result_t result;
        xQueueReceive(evt_queue, &result, portMAX_DELAY);

        // Extract labels from the detection result
        String labels = "";
        for (size_t i = 0; i < result.bounding_boxes_count; i++) {
            auto bb = result.bounding_boxes[i];
            if (bb.value > 0) {
                labels += bb.label;
                labels += ",";
            }
        }
        labels.remove(labels.length() - 1); // Remove the last comma

        // Publish the labels over MQTT
        if (mqtt_client.connected()) {
            mqtt_client.publish(MQTT_HB_TOPIC, labels.c_str());
        }
    }
}

// Print memory information
void print_memory() {
    ESP_LOGI(TAG, "Total heap: %u", ESP.getHeapSize());
    ESP_LOGI(TAG, "Free heap: %u", ESP.getFreeHeap());
    ESP_LOGI(TAG, "Total PSRAM: %u", ESP.getPsramSize());
    ESP_LOGI(TAG, "Free PSRAM: %d", ESP.getFreePsram());
}

// Prepare feature
void ei_prepare_feature(uint8_t *img_buf, signal_t *signal) {
    signal->total_length = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT;
    signal->get_data = &ei_get_feature_callback;
    if ((EI_CAMERA_RAW_FRAME_BUFFER_ROWS != EI_CLASSIFIER_INPUT_WIDTH) || (EI_CAMERA_RAW_FRAME_BUFFER_COLS != EI_CLASSIFIER_INPUT_HEIGHT)) {
        ei::image::processing::crop_and_interpolate_rgb888(
            img_buf,
            EI_CAMERA_RAW_FRAME_BUFFER_COLS,
            EI_CAMERA_RAW_FRAME_BUFFER_ROWS,
            img_buf,
            EI_CLASSIFIER_INPUT_WIDTH,
            EI_CLASSIFIER_INPUT_HEIGHT);
    }
}

// Get feature callback
int ei_get_feature_callback(size_t offset, size_t length, float *out_ptr) {
    size_t pixel_ix = offset * 3;
    size_t pixels_left = length;
    size_t out_ptr_ix = 0;

    while (pixels_left != 0) {
        out_ptr[out_ptr_ix] = (bmp_buf[pixel_ix] << 16) + (bmp_buf[pixel_ix + 1] << 8) + bmp_buf[pixel_ix + 2];

        // Go to the next pixel
        out_ptr_ix++;
        pixel_ix += 3;
        pixels_left--;
    }
    return 0;
}

// Use result from classifier
int32_t ei_use_result(ei_impulse_result_t result) {
    int32_t detection = 0;
    ESP_LOGI(TAG, "Predictions (DSP: %d ms., Classification: %d ms., Anomaly: %d ms.)",
             result.timing.dsp, result.timing.classification, result.timing.anomaly);
    bool bb_found = result.bounding_boxes[0].value > 0;
    for (size_t ix = 0; ix < result.bounding_boxes_count; ix++) {
        auto bb = result.bounding_boxes[ix];
        if (bb.value == 0) {
            continue;
        }
        ESP_LOGI(TAG, "%s (%f) [ x: %u, y: %u, width: %u, height: %u ]", bb.label, bb.value, bb.x, bb.y, bb.width, bb.height);
        detection++;
    }
    if (!bb_found) {
        ESP_LOGI(TAG, "No objects found");
    } else {
        ESP_LOGI(TAG, "Object detected: %s", result.bounding_boxes[0].label);
    }
    return detection;
}

// Initialize hardware
void setup() {
    Serial.begin(115200);
    print_memory();
    hw_camera_init();
    bmp_buf = (uint8_t*)ps_malloc(BMP_BUF_SIZE);
    if (psramInit()) {
        ESP_LOGI(TAG, "PSRAM initialized");
    } else {
        ESP_LOGE(TAG, "PSRAM not available");
    }
    evt_queue = xQueueCreate(10, sizeof(ei_impulse_result_t));

    // Create tasks
    xTaskCreate(
        EI_objDetection,    // Task function
        "EI_objDetection",  // Name of task
        4096,               // Stack size of task
        NULL,               // Parameter of the task
        3,                  // Priority of the task
        NULL                // Task handle to keep track of created task
    );

    xTaskCreate(
        comm_task,    // Task function
        "comm_task",  // Name of task
        8192,         // Stack size of task
        NULL,         // Parameter of the task
        2,            // Priority of the task
        NULL          // Task handle to keep track of created task
    );
}

// Main loop
void loop() {
    if (mqtt_client.connected()) {
        mqtt_client.loop();
        Serial.print("MQTT loop\n");
    }
    delay(3000);
}
