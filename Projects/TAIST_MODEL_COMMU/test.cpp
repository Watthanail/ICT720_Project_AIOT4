////////////////////////////////////////////
// DEFINE
/////////////////////////////////////////////

#define VERSION "1.0"
#define LED_BUILTIN 2

// CONFIG
#define CFG_NODE_ID "node-01"
#define CFG_WIFI_SSID "EmOne_2.4G"
#define CFG_WIFI_PASS "nice2meetu"
#define CFG_MQTT_SERVER "104.248.148.216"
#define CFG_MQTT_PORT 1883
#define CFG_MQTT_USER "Train01"
#define CFG_MQTT_PASS "Train01"

/////////////////////////////////////////////
// INCLUDE
/////////////////////////////////////////////

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "hw_camera.h"

// CONFIG
#include "ArduinoJson.h"


// WIFI


#include <WiFi.h>
#include <WiFiMulti.h>


// MQTT
#include <AsyncMqttClient.h>

#include <Workshop01_inferencing.h>
#include "edge-impulse-sdk/dsp/image/image.hpp"

/////////////////////////////////////////////
// GLOBAL VARIABLES
/////////////////////////////////////////////
// CONFIG
typedef struct {
  char node_id[64] = CFG_NODE_ID;
  char wifi_ssid[64] = CFG_WIFI_SSID;
  char wifi_pass[64] = CFG_WIFI_PASS;
  char mqtt_server[64] = CFG_MQTT_SERVER;
  uint16_t mqtt_port = CFG_MQTT_PORT;
  char mqtt_user[64] = CFG_MQTT_USER;
  char mqtt_pass[64] = CFG_MQTT_PASS;
  uint32_t uplink_interval = 30000;
  uint32_t sensor_interval = 2000;
} config_t;

config_t cfg;

// SENSOR VALUE
typedef struct {
  uint8_t motor1 = 0;
  uint8_t motor2 = 0;
  uint16_t waterLevel1 = 0; // div by 10
  uint16_t waterLevel2 = 0; // div by 10
  uint8_t ph = 71; // div 10
  uint8_t flow = 0;
} sensor_d;

sensor_d sensor;

TimerHandle_t sensorTimer;


// UPLINK/DOWNLINK
TimerHandle_t uplinkTimer;

// WIFI
TimerHandle_t wifiReconnectTimer;


// MQTT
AsyncMqttClient mqttClient;
TimerHandle_t mqttReconnectTimer;

char mqtt_id[64] = "node_%s";
char mqtt_topic_cmd[64] = "Train01/%s/cmd";
char mqtt_topic_data[64] = "Train01/%s/data";
char mqtt_topic_cfg[64] = "Train01/%s/data";

// OTA
WiFiClient otaClient;


/////////////////////////////////////////////
// SENSOR / UPLINK / DOWNLINK
/////////////////////////////////////////////
void sensor_setup();
void sensor_update();
void uplink_start();
void downlink_callback(char* topic, uint8_t* payload, size_t len);
void downlink_command(uint8_t* payload);

void wifi_setup();
void wifi_connect();
void wifi_event(WiFiEvent_t event);
void mqtt_setup();
void mqtt_connect();
void mqtt_onConnect(bool sessionPresent);
void mqtt_onDisconnect(AsyncMqttClientDisconnectReason reason);
void mqtt_onMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);


/////////////////////////////////////////////
// SENSOR / UPLINK / DOWNLINK
/////////////////////////////////////////////
void sensor_setup() {
  // TODO: add sensor setup
  uplinkTimer = xTimerCreate("uplinkTimer", pdMS_TO_TICKS(cfg.uplink_interval), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(uplink_start));
  
  sensorTimer = xTimerCreate("sensorTimer", pdMS_TO_TICKS(cfg.sensor_interval), pdTRUE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(sensor_update));
  xTimerStart(sensorTimer, 0);
}

void sensor_update() {
  sensor.waterLevel1 = random(400, 1900); // 40.0 - 190.0
  sensor.waterLevel2 = random(400, 1900); // 40.0 - 190.0
  sensor.ph = random(60, 80); // 6.0 - 8.0
  sensor.flow = 1;
}

void uplink_start() {
  Serial.println("UPLINK: START");
  xTimerStart(uplinkTimer, 0);

  char payload[200];
  sprintf(payload, "{\"motor1\":%d,\"motor2\":%d,\"waterLevel1\":%.1f,\"waterLevel2\":%.1f,\"ph\":%.1f,\"flow\":%.1f}",
    sensor.motor1, sensor.motor2,
    sensor.waterLevel1 / 10.0f, sensor.waterLevel2 / 10.0f, 
    sensor.ph / 10.0f, sensor.flow / 10.0f
  );
  Serial.print("UPLINK: topic=");
  Serial.println(mqtt_topic_data);
  Serial.print("UPLINK: payload=");
  Serial.println(payload);
  Serial.println(cfg.uplink_interval);
  
  mqttClient.publish(mqtt_topic_data, 0, false, payload);
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
  WiFi.begin(cfg.wifi_ssid, cfg.wifi_pass);
    while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  }
  digitalWrite(LED_BUILTIN, HIGH);
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
  mqttClient.setServer(cfg.mqtt_server, cfg.mqtt_port);
  mqttClient.setCredentials(cfg.mqtt_user, cfg.mqtt_pass);

  sprintf(mqtt_topic_data, "Train01/%s/data", cfg.node_id);

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


/////////////////////////////////////////////
//
/////////////////////////////////////////////
void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  while(!Serial);
  Serial.print("VER: ");
  Serial.println(VERSION);
  wifi_setup();
  mqtt_setup();
  sensor_setup();
}

void loop() {
}
