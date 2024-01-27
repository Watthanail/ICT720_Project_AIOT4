#include <Arduino.h>
#include <task.h>
#include <queue.h>
#include "hw_mic.h"
 
// queue handle
QueueHandle_t evt_queue;
 
// task function
void sound_detect_task(void *pvParameter) {
  static int32_t samples[160];
  static unsigned int num_samples = 160;
  float avg_sample, base_sample;
 
  // initialize microphone
  hw_mic_init(16000);
 
  // prepare baseline with 100 rounds
  base_sample = 0;
  for (int i=0; i < 100; i++) {
    num_samples = 160;
    hw_mic_read(samples, &num_samples);
    avg_sample = 0;
    for (int j=0; j < num_samples; j++) {
      avg_sample += abs(samples[j]);
    }
    avg_sample /= num_samples;
    base_sample += avg_sample;
  }
  base_sample /= 100;
 
  // read microphone
  while (true) {
    num_samples = 160;
    hw_mic_read(samples, &num_samples);
    // do something with samples
    avg_sample = 0;
    for (int j=0; j < num_samples; j++) {
        avg_sample += abs(samples[j]);
    }
 
    avg_sample /= num_samples;
    avg_sample /= base_sample;
 
    // send event to comm task
    xQueueSend(evt_queue, &avg_sample, portMAX_DELAY);
 
  }
}
 
// comm task
void comm_task(void *pvParameter) {
  float value;
 
  //buf
  const int BUF_SIZE = 50;
  float buf[50];
  int buf_idx = 0;
  bool detected = false;
  float avg_value;
 
  Serial.begin(115200);
  for (int i=0; i < BUF_SIZE; i++) {
    buf[i] = 0;
 
  }
  while (true) {
 
    // wait for event
    xQueueReceive(evt_queue, &value, portMAX_DELAY);
 
    //update circular buffer
    buf[buf_idx] = value;  
    buf_idx = (buf_idx + 1) % BUF_SIZE;
 
    //do miving average
    avg_value = 0;
    for (int i=0; i < BUF_SIZE; i++) {
      avg_value += buf[i];
    }
    avg_value /= BUF_SIZE;
 
    //do threshold
    if (avg_value > 0.5){
      detected = true;
    } else {
      detected = false;
    }
    // do something with event
    Serial.printf("%d, %f, %d\n",millis(), value, detected);
  }
}
 
void setup() {
  // initialize RTOS task
  evt_queue = xQueueCreate(10, sizeof(float));
 
  xTaskCreate(
    sound_detect_task,    // task function
    "hw_mic_task",  // name of task   สร้าง task ที่มี function ชื่อเดียวกันได้
    4096,           // stack size of task    4096 bytes of memory for each stack
    NULL,           // parameter of the task   
    3,              // priority of the task   0 = lowest priority = idle
    NULL           // task handle to keep track of created task
  );
 
  xTaskCreate(
    comm_task,    // task function
    "comm_task",  // name of task
    4096,         // stack size of task
    NULL,         // parameter of the task
    2,            // priority of the task / 2 more important than 3
    NULL          // task handle to keep track of created task
  );
}
 
void loop() {
  delay(100);
}