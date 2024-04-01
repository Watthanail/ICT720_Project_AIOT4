////////////////////////////////////////////
// DEFINE
////////////////////////////////////////////
#define VERSION "1.0"

// FreeRTOS
#include <Arduino.h>
#include <Task.h>
#include <Queue.h>

// WIFI
#include <WiFi.h>
#include <WiFiClient.h>

// MQTT
#include <PubSubClient.h>

// Edge Impulse
#include "hw_camera.h"
#include <Workshop01_inferencing.h>
#include "edge-impulse-sdk/dsp/image/image.hpp"

/////////////////////////////////////////////
// GLOBAL VARIABLES
/////////////////////////////////////////////

#define CFG_WIFI_SSID "xxxxx"
#define CFG_WIFI_PASS "xxxx"
#define CFG_MQTT_SERVER "emqx.taist.online"
#define CFG_MQTT_PORT 1883
#define CFG_MQTT_USER "xxx"
#define CFG_MQTT_PASS "xxxxx"
#define MQTT_HB_TOPIC "taist2024/aiot/heartbeat/dev_8" // public

#define EI_CAMERA_RAW_FRAME_BUFFER_COLS 240
#define EI_CAMERA_RAW_FRAME_BUFFER_ROWS 240
#define EI_CAMERA_FRAME_BYTE_SIZE 3
#define BMP_BUF_SIZE (EI_CAMERA_RAW_FRAME_BUFFER_COLS * EI_CAMERA_RAW_FRAME_BUFFER_ROWS * EI_CAMERA_FRAME_BYTE_SIZE)
#define BBOX_INFO_SIZE 10
#define MAX_BBOX_INFO_LENGTH 64

static uint8_t *bmp_buf;
uint32_t width = 240;
uint32_t height = 240;

WiFiClient wifi_client;
PubSubClient mqtt_client(wifi_client);
char bbox_info[BBOX_INFO_SIZE][MAX_BBOX_INFO_LENGTH];

QueueHandle_t evt_queue;

// Function prototypes
void ei_prepare_feature(uint8_t *img_buf, signal_t *signal);
int ei_get_feature_callback(size_t offset, size_t length, float *out_ptr);
void ei_use_result(ei_impulse_result_t result);

void comm_task(void *pvParameter);
void cam_detect_task(void *pvParameter);

void setup()
{
  Serial.begin(115200);
  WiFi.begin(CFG_WIFI_SSID, CFG_WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      Serial.println("Connecting to WiFi...");
  }
  mqtt_client.setServer(CFG_MQTT_SERVER, CFG_MQTT_PORT);
  while (!mqtt_client.connected()) {
      if (mqtt_client.connect("taist_watthanai", CFG_MQTT_USER, CFG_MQTT_PASS)) {
          Serial.println("Connected to MQTT broker");
      } else {
          Serial.print("Failed to connect to MQTT broker, rc=");
          Serial.println(mqtt_client.state());
          delay(2000);
      }
  }

  // // evt_queue = xQueueCreate(10, sizeof(float));

  xTaskCreatePinnedToCore(
      comm_task,      // task function
      "comm_task",    // name of task
      4096,           // stack size of task
      nullptr,        // parameter of the task
      2,              // priority of the task
      nullptr,        // task handle to keep track of created task
      0               // core to run the task on (0 or 1)
  );

  xTaskCreatePinnedToCore(
      cam_detect_task,   // task function
      "cam_detect_task", // name of task
      8192,              // stack size of task
      nullptr,           // parameter of the task
      3,                 // priority of the task
      nullptr,           // task handle to keep track of created task
      1                  // core to run the task on (0 or 1)
  );
}

void loop()
{
  // Empty because all tasks are handled in FreeRTOS tasks
}

void comm_task(void *pvParameter)
{
  while (true)
  {
    float value;
    char payload[500];
    snprintf(payload, sizeof(payload), "{\"ID\": %s ,\"timestamp\":%lu,\"valve\":%.2f}",
             "6614552627",
             millis(),
             27.27);

    if (mqtt_client.connected())
    {
      mqtt_client.publish(MQTT_HB_TOPIC, payload);
    }

    vTaskDelay(1000 / portTICK_PERIOD_MS); // Delay for 1 second
  }
}

void cam_detect_task(void *pvParameter)
{
  hw_camera_init();
  bmp_buf = (uint8_t *)ps_malloc(BMP_BUF_SIZE);
  while (true)
  {
    hw_camera_raw_snapshot(bmp_buf, &width, &height); // Pass width and height parameters
    ei::signal_t signal;
    ei_prepare_feature(bmp_buf, &signal);
    ei_impulse_result_t result = {0};
    bool debug_nn = false;
    run_classifier(&signal, &result, debug_nn);

    bool bb_found = result.bounding_boxes[0].value > 0;
    for (size_t ix = 0; ix < result.bounding_boxes_count; ix++)
    {
      auto bb = result.bounding_boxes[ix];
      if (bb.value == 0)
      {
        continue;
      }
      snprintf(bbox_info[ix], MAX_BBOX_INFO_LENGTH, "%s (%f) [ x: %u, y: %u, width: %u, height: %u ]",
               bb.label, bb.value, bb.x, bb.y, bb.width, bb.height);
      Serial.println(bbox_info[ix]);
    }

    if (!bb_found)
    {
      snprintf(bbox_info[0], MAX_BBOX_INFO_LENGTH, "No objects found");
      Serial.println("No objects found");
    }

    vTaskDelay(2000 / portTICK_PERIOD_MS); // Delay for 2 seconds
  }
}

// Prepare feature
void ei_prepare_feature(uint8_t *img_buf, signal_t *signal)
{
  signal->total_length = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT;
  signal->get_data = &ei_get_feature_callback;
  if ((EI_CAMERA_RAW_FRAME_BUFFER_ROWS != EI_CLASSIFIER_INPUT_WIDTH) || (EI_CAMERA_RAW_FRAME_BUFFER_COLS != EI_CLASSIFIER_INPUT_HEIGHT))
  {
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
int ei_get_feature_callback(size_t offset, size_t length, float *out_ptr)
{
  size_t pixel_ix = offset * 3;
  size_t pixels_left = length;
  size_t out_ptr_ix = 0;

  while (pixels_left != 0)
  {
    out_ptr[out_ptr_ix] = (bmp_buf[pixel_ix] << 16) + (bmp_buf[pixel_ix + 1] << 8) + bmp_buf[pixel_ix + 2];

    // Go to the next pixel
    out_ptr_ix++;
    pixel_ix += 3;
    pixels_left--;
  }
  return 0;
}

// Use result from classifier
void ei_use_result(ei_impulse_result_t result)
{
  Serial.printf("Predictions (DSP: %d ms., Classification: %d ms., Anomaly: %d ms.)\n",
                result.timing.dsp, result.timing.classification, result.timing.anomaly);

  bool bb_found = result.bounding_boxes[0].value > 0;
  for (size_t ix = 0; ix < result.bounding_boxes_count; ix++)
  {
    auto bb = result.bounding_boxes[ix];
    if (bb.value == 0)
    {
      continue;
    }

    // Copy data to bbox_info strings
    snprintf(bbox_info[ix], MAX_BBOX_INFO_LENGTH, "%s (%f) [ x: %u, y: %u, width: %u, height: %u ]",
             bb.label, bb.value, bb.x, bb.y, bb.width, bb.height);

    Serial.println(bbox_info[ix]); // Print the bounding box info
  }

  if (!bb_found)
  {
    Serial.println("No objects found");

    // Provide default information if no objects found
    snprintf(bbox_info[0], MAX_BBOX_INFO_LENGTH, "No objects found");
  }
}
