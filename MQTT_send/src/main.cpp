#include "main.h"
#include "hw_camera.h"
#include <ICT720_Project_AIOT4_inferencing.h> // project name
#include "edge-impulse-sdk/dsp/image/image.hpp"
#include <Arduino.h>
#include <task.h>
#include <queue.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>

// constants 
#define TAG     "main"

// camera
#define BTN_PIN 0

#define EI_CAMERA_RAW_FRAME_BUFFER_COLS           240
#define EI_CAMERA_RAW_FRAME_BUFFER_ROWS           240
#define EI_CAMERA_FRAME_BYTE_SIZE                 3
#define BMP_BUF_SIZE                             (EI_CAMERA_RAW_FRAME_BUFFER_COLS * EI_CAMERA_RAW_FRAME_BUFFER_ROWS * EI_CAMERA_FRAME_BYTE_SIZE)

// MQTT
#define MQTT_BROKER "broker.hivemq.com"
#define MQTT_PORT 1883
#define MQTT_DETECT_TOPIC "taist/aiot/detect/Dev01" // public
#define MQTT_CMD_TOPIC "taist/aiot/command/Dev01"  // subscribe

// WIFI
#define WIFI_SSID "XXX"
#define WIFI_PASSWORD "XXX"


// global variables
WiFiClient wifi_client;
PubSubClient mqtt_client(wifi_client);

// queue handle
QueueHandle_t evt_queue;

// static variables
static uint8_t *bmp_buf;

// static function prototypes
void print_memory(void);
void on_cmd_received(char *topic, byte *payload, unsigned int length);
void comm_task(void *pvParameter);
void detect_object_task(void *pvParameter);
void ei_prepare_feature(uint8_t *img_buf, signal_t *signal);
int ei_get_feature_callback(size_t offset, size_t length, float *out_ptr);
void ei_use_result(ei_impulse_result_t result);

// initialize hardware
void setup() {
  Serial.begin(115200);\
  print_memory();
  hw_camera_init();
  bmp_buf = (uint8_t*)ps_malloc(BMP_BUF_SIZE);
  if (psramInit()) {
    ESP_LOGI(TAG, "PSRAM initialized");
  } else {
    ESP_LOGE(TAG, "PSRAM not available");
  }
  // initialize RTOS task
  evt_queue = xQueueCreate(10, sizeof(float));
  // create tasks
  xTaskCreate(
      detect_object_task, // task function
      "detect_object_task",     // name of task
      4096,              // stack size of task
      NULL,              // parameter of the task
      3,                 // priority of the task
      NULL               // task handle to keep track of created task
  );

  xTaskCreate(
      comm_task,   // task function
      "comm_task", // name of task
      4096,        // stack size of task
      NULL,        // parameter of the task
      2,           // priority of the task
      NULL         // task handle to keep track of created task
  );
  // try to connect to wifi
  Serial.println("Initializing WiFi Connection...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Successfully connect");
  }
}

// main loop
void loop() {
  // execute MQTT loop
  if (mqtt_client.connected())
  {
    mqtt_client.loop();
  }
  delay(100);
}

// Print memory information
void print_memory() {
  ESP_LOGI(TAG, "Total heap: %u", ESP.getHeapSize());
  ESP_LOGI(TAG, "Free heap: %u", ESP.getFreeHeap());
  ESP_LOGI(TAG, "Total PSRAM: %u", ESP.getPsramSize());
  ESP_LOGI(TAG, "Free PSRAM: %d", ESP.getFreePsram());
}

void on_cmd_received(char *topic, byte *payload, unsigned int length){
}

void comm_task(void *pvParameter){
  char *value;
  const int MAX_STR_LEN = 10;
  char buf[MAX_STR_LEN];
  static uint32_t prev_ms = 0; // last ms of sending MQTT

  // initialize serial and network
  Serial.begin(115200);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  mqtt_client.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt_client.setCallback(on_cmd_received);

  while (true) {
    // wait for event
    xQueueReceive(evt_queue, &value, portMAX_DELAY);
    // update buffer
    strcpy(buf, value);
    // communicate with MQTT
    Serial.printf("%d, %s\n", millis(), value);
    if (millis() - prev_ms > 2000){
      prev_ms = millis();
      if (mqtt_client.connected()){
        mqtt_client.publish(MQTT_DETECT_TOPIC, buf);
      }
    }
  }
}


// Detect object
void detect_object_task(void *pvParameter){
  static bool press_state = false;
  static uint32_t prev_millis = 0;

  if (digitalRead(BTN_PIN) == 0) {
    if ((millis() - prev_millis > 500) && (press_state == false)) {
      uint32_t Tstart, elapsed_time;
      uint32_t width, height;

      prev_millis = millis();
      Tstart = millis();
      // get raw data
      ESP_LOGI(TAG, "Taking snapshot...");
      hw_camera_raw_snapshot(bmp_buf, &width, &height);
      elapsed_time = millis() - Tstart;
      ESP_LOGI(TAG, "Snapshot taken (%d) width: %d, height: %d", elapsed_time, width, height);
      print_memory();
      // prepare feature
      Tstart = millis();
      ei::signal_t signal;      
      ei_prepare_feature(bmp_buf, &signal);
      elapsed_time = millis() - Tstart;
      ESP_LOGI(TAG, "Feature taken (%d)", elapsed_time);
      print_memory();
      // run classifier
      Tstart = millis();
      ei_impulse_result_t result = { 0 };
      bool debug_nn = false;
      run_classifier(&signal, &result, debug_nn);
      elapsed_time = millis() - Tstart;
      ESP_LOGI(TAG, "Classification done (%d)", elapsed_time);
      print_memory();
      // use result
      ei_use_result(result);
      press_state = true;
    } 
  } else {
    if (press_state) {
      press_state = false;
    }
  }
}




// prepare feature
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

// get feature callback
int ei_get_feature_callback(size_t offset, size_t length, float *out_ptr) {
  size_t pixel_ix = offset * 3;
  size_t pixels_left = length;
  size_t out_ptr_ix = 0;

  while (pixels_left != 0) {
    out_ptr[out_ptr_ix] = (bmp_buf[pixel_ix] << 16) + (bmp_buf[pixel_ix + 1] << 8) + bmp_buf[pixel_ix + 2];

    // go to the next pixel
    out_ptr_ix++;
    pixel_ix+=3;
    pixels_left--;
  }
  return 0;
}

// use result from classifier
void ei_use_result(ei_impulse_result_t result) {
  ESP_LOGI(TAG, "Predictions (DSP: %d ms., Classification: %d ms., Anomaly: %d ms.)",
    result.timing.dsp, result.timing.classification, result.timing.anomaly);
  bool bb_found = result.bounding_boxes[0].value > 0;
  for (size_t ix = 0; ix < result.bounding_boxes_count; ix++) {
    auto bb = result.bounding_boxes[ix];
    if (bb.value == 0) {
      continue;
    }
    ESP_LOGI(TAG, "%s (%f) [ x: %u, y: %u, width: %u, height: %u ]", bb.label, bb.value, bb.x, bb.y, bb.width, bb.height);
    // send event to comm task
    xQueueSend(evt_queue, &bb.label, portMAX_DELAY);
  }
  if (!bb_found) {
    ESP_LOGI(TAG, "No objects found");
  } 
}