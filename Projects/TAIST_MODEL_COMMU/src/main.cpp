////////////////////////////////////////////
// DEFINE
////////////////////////////////////////////
#define VERSION "1.0"

// Freertos
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#include "ArduinoJson.h"

// WIFI
#include <WiFi.h>
#include <WiFiMulti.h>

// MQTT
#include <AsyncMqttClient.h>

// Edge Impulse
#include "hw_camera.h"
#include <Workshop01_inferencing.h>
#include "edge-impulse-sdk/dsp/image/image.hpp"

/////////////////////////////////////////////
// GLOBAL VARIABLES
/////////////////////////////////////////////

// CONFIG COMMUNICATION//
#define CFG_NODE_ID "xxxxxxx"
#define CFG_WIFI_SSID "xxxxxx"
#define CFG_WIFI_PASS "xxxxx"
#define CFG_MQTT_SERVER "xxxxxxx"
#define CFG_MQTT_PORT 1883
#define CFG_MQTT_USER "xxxxxxx"
#define CFG_MQTT_PASS "xxxxxx"

// CONFIG Edge Impulse//
#define CFG_FRAME_BUFFER_COLS 240
#define CFG_FRAME_BUFFER_ROWS 240
#define CFG_FRAME_BYTE_SIZE 3
#define CFG_BMP_BUF_SIZE (CFG_FRAME_BUFFER_COLS * CFG_FRAME_BUFFER_ROWS * CFG_FRAME_BYTE_SIZE)
#define CFG_BBOX_INFO_SIZE 10
#define CFG_MAX_BBOX_INFO_LENGTH 64

typedef struct {
    char node_id[64] = CFG_NODE_ID ;
    char wifi_ssid[64] = CFG_WIFI_SSID;
    char wifi_pass[64] = CFG_WIFI_PASS;
    char mqtt_server[64] = CFG_MQTT_SERVER;
    uint16_t mqtt_port = CFG_MQTT_PORT;
    char mqtt_user[64] = CFG_MQTT_USER;
    char mqtt_pass[64] = CFG_MQTT_PASS;
    uint32_t uplink_interval = 10000;
    bool read_data = false;
} commu_config_t;

commu_config_t commu_cfg;


typedef struct {
    uint8_t BUFFER_COLS = CFG_FRAME_BUFFER_COLS;
    uint8_t BUFFER_ROWS = CFG_FRAME_BUFFER_ROWS;
    uint16_t BYTE_SIZE = CFG_FRAME_BYTE_SIZE;
    uint16_t BUF_SIZE = CFG_BMP_BUF_SIZE;
    uint8_t *bmp_buf;
    uint32_t jpg_width = 240;
    uint32_t jpg_height = 240;
    uint8_t jpg_buf[20480];
    uint16_t jpg_sz = 0;
    uint32_t cam_interval = 1000;
} Edge_config_t;

Edge_config_t EI_cfg;

// Edge Impulse
TimerHandle_t CamTimer;

// UPLINK/DOWNLINK
TimerHandle_t uplinkTimer;

// WIFI
TimerHandle_t wifiReconnectTimer;

// MQTT
AsyncMqttClient mqttClient;
TimerHandle_t mqttReconnectTimer;

char mqtt_topic_data[64] = "taist2024/%s/data";
char mqtt_topic_cmd[64] = "taist2024/%s/cmd";

char bbox_info[CFG_BBOX_INFO_SIZE][CFG_MAX_BBOX_INFO_LENGTH]; // Fixed-size char arrays for bounding box info
/////////////////////////////////////////////
// Camera / UPLINK / DOWNLINK
/////////////////////////////////////////////

void cam_setup();
void cam_update();
void uplink_start();
void wifi_setup();
void wifi_connect();
void wifi_event(WiFiEvent_t event);
void mqtt_setup();
void mqtt_connect();
void mqtt_onConnect(bool sessionPresent);
void mqtt_onDisconnect(AsyncMqttClientDisconnectReason reason);
void mqtt_onMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);
void downlink_callback(char* topic, uint8_t* payload, size_t len);
void downlink_command(uint8_t* payload);
void ei_prepare_signal(uint8_t *img_buf, ei::signal_t *signal);
void ei_handle_result(ei_impulse_result_t result);
int ei_get_data_callback(size_t offset, size_t length, float *out_ptr);



/////////////////////////////////////////////
// WIFI
/////////////////////////////////////////////
void wifi_setup() {
  Serial.println("WIFI: SETUP");
  wifiReconnectTimer = xTimerCreate("wifiTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(wifi_connect));
  WiFi.onEvent(wifi_event);
  wifi_connect();
}

void wifi_connect() {
  Serial.println("WIFI: Connecting...");
  WiFi.begin(commu_cfg.wifi_ssid, commu_cfg.wifi_pass);
    while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("WIFI : Connected");
}


void wifi_event(WiFiEvent_t event) {
  Serial.print("WIFI: event=");
  Serial.println(event);
  
  switch(event) {
  case SYSTEM_EVENT_STA_GOT_IP:
    Serial.println("WIFI: Connected");
    Serial.print("WIFI: IP address=");
    Serial.println(WiFi.localIP());
    mqtt_connect();
    break;
  case SYSTEM_EVENT_STA_DISCONNECTED:
    Serial.println("WIFI: Lost connection");
    xTimerStop(mqttReconnectTimer, 0);
    xTimerStart(wifiReconnectTimer, 0);
    break;
  }
}


/////////////////////////////////////////////
// MQTT
/////////////////////////////////////////////


void mqtt_setup() {
  Serial.println("MQTT: SETUP");
  
  mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(mqtt_connect));

  mqttClient.onConnect(mqtt_onConnect);
  mqttClient.onDisconnect(mqtt_onDisconnect);
  mqttClient.onMessage(mqtt_onMessage);
  mqttClient.setServer(commu_cfg.mqtt_server, commu_cfg.mqtt_port);
  mqttClient.setCredentials(commu_cfg.mqtt_user, commu_cfg.mqtt_pass);

  sprintf(mqtt_topic_data, "taist2024/%s/data", commu_cfg.node_id);

}

void mqtt_connect() {
  Serial.println("MQTT: Connecting...");
  mqttClient.connect();
}


void mqtt_onConnect(bool sessionPresent) {
  Serial.println("MQTT: Connected");
  Serial.print("MQTT: Session present: ");
  Serial.println(sessionPresent);
  
  mqttClient.subscribe(mqtt_topic_cmd, 0);

  uplink_start();
}

void mqtt_onDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println("Disconnected from MQTT.");
  xTimerStop(uplinkTimer, 0);
  
  if (WiFi.isConnected()) {
    xTimerStart(mqttReconnectTimer, 0);
  }
}



void mqtt_onMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {

  downlink_callback(topic, (uint8_t *)payload, len);
}

void downlink_callback(char* topic, uint8_t* payload, size_t len) {
  // TODO: process downlink here
  Serial.print("DOWLINK: topic=");
  Serial.println(topic);
  payload[len] = 0;
  Serial.print("DOWLINK: payload=");
  Serial.println((char*) payload);

  uint8_t topic_len = strlen(topic);
  if (topic[topic_len - 3] == 'c' && topic[topic_len - 2] == 'm' && topic[topic_len - 1] == 'd') {
    downlink_command(payload);
  }
}

void downlink_command(uint8_t* payload) {
  bool restart = false;
  DynamicJsonDocument doc(1024);
  deserializeJson(doc, (char*)payload);
  bool uplink = false;
  
}


void cam_setup() {
  
  uplinkTimer = xTimerCreate(
                "uplinkTimer", 
                 pdMS_TO_TICKS(commu_cfg.uplink_interval),
                 pdFALSE, 
                 (void*)0, 
                 reinterpret_cast<TimerCallbackFunction_t>(uplink_start));
  
  CamTimer = xTimerCreate(
                "CamTimer",
                 pdMS_TO_TICKS(EI_cfg.cam_interval),
                 pdTRUE, 
                 (void*)0, 
                 reinterpret_cast<TimerCallbackFunction_t>(cam_update));
  xTimerStart(CamTimer, 0);
}

void cam_update() {
  Serial.println("Taking snapshot...");
  hw_camera_raw_snapshot(EI_cfg.bmp_buf, &EI_cfg.jpg_width, &EI_cfg.jpg_height);
  ei::signal_t signal;
  ei_prepare_signal(EI_cfg.bmp_buf, &signal);
  ei_impulse_result_t result = {0};
  bool debug_nn = false;
  run_classifier(&signal, &result, debug_nn);
  ei_handle_result(result);
}

void uplink_start(){
  // Your uplink logic here
}

// Prepare signal for inference
void ei_prepare_signal(uint8_t *img_buf, ei::signal_t *signal) {
  signal->total_length = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT;
  signal->get_data = &ei_get_data_callback;
  
  if ((EI_cfg.BUFFER_ROWS != EI_CLASSIFIER_INPUT_WIDTH) || (EI_cfg.BUFFER_COLS != EI_CLASSIFIER_INPUT_HEIGHT)) {
    ei::image::processing::crop_and_interpolate_rgb888(
      img_buf,
      EI_cfg.BUFFER_COLS,
      EI_cfg.BUFFER_ROWS,
      img_buf,
      EI_CLASSIFIER_INPUT_WIDTH,
      EI_CLASSIFIER_INPUT_HEIGHT
    );
  }
}

// Get data callback for feature extraction
int ei_get_data_callback(size_t offset, size_t length, float *out_ptr) {
  size_t pixel_ix = offset * 3;
  size_t pixels_left = length;
  size_t out_ptr_ix = 0;

  while (pixels_left != 0) {
    out_ptr[out_ptr_ix] = (EI_cfg.bmp_buf[pixel_ix] << 16) + (EI_cfg.bmp_buf[pixel_ix + 1] << 8) + EI_cfg.bmp_buf[pixel_ix + 2];
    out_ptr_ix++;
    pixel_ix += 3;
    pixels_left--;
  }
  return 0;
}

// Handle result from classifier
void ei_handle_result(ei_impulse_result_t result) {
  Serial.printf("Predictions (DSP: %d ms., Classification: %d ms., Anomaly: %d ms.)\n",
                result.timing.dsp, result.timing.classification, result.timing.anomaly);

  bool bb_found = result.bounding_boxes[0].value > 0;
  for (size_t ix = 0; ix < result.bounding_boxes_count; ix++) {
    auto bb = result.bounding_boxes[ix];
    if (bb.value == 0) {
      continue;
    }

    // Copy data to bbox_info strings
    snprintf(bbox_info[ix], CFG_MAX_BBOX_INFO_LENGTH, "%s (%f) [ x: %u, y: %u, width: %u, height: %u ]",
             bb.label, bb.value, bb.x, bb.y, bb.width, bb.height);

    Serial.println(bbox_info[ix]); // Print the bounding box info
  }

  if (!bb_found) {
    Serial.println("No objects found");

    // Provide default information if no objects found
    snprintf(bbox_info[0], CFG_MAX_BBOX_INFO_LENGTH, "No objects found");
  }
}

void setup(){
    Serial.begin(115200);
    Serial.print("VER: ");
    Serial.println(VERSION);
    wifi_setup();
    mqtt_setup();
    cam_setup();


}

void loop(){

}
