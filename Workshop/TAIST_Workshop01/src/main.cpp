#include <Arduino.h>
#include <esp_log.h>
#include "hw_mic.h"
 
#define TAG "main"
 
void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  hw_mic_init(16000);
}
 
void loop() {
  // put your main code here, to run repeatedly:
  //Serial.println("Hello World!"); //make sure the buildthrough is running well
  static uint32_t counter = 0; //static: persistent inside the scope - allows you to make something look global, exists all the time for all functions inside the file
  static int32_t samples[1024]; //buffer to contact with mic
  static uint32_t num_samples = 1024; //buffer to contact with mic
  ESP_LOGI(TAG, "Hello world! %d", counter++); //LOGI: Log Info...if DCORE_DEBUG_LEVEL=0 this line will be skipped
  hw_mic_read(samples, &num_samples);
  uint32_t avg_sound = 0;
  for (int i=0; i<num_samples; i++){
    avg_sound += abs(samples[i]);
  }
  avg_sound /= num_samples;
  Serial.println(avg_sound);
  delay(1);
}