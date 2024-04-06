#include "main.h"
#include "hw_camera.h"
#include "openmvrpc.h"

#define TAG "main"

#define BUTTON_PIN 21
#define LED_PIN 45

#include <Arduino.h>
#include <Task.h>
#include <Queue.h>

// Edge Impulse
#include "hw_camera.h"
#include <Workshop01_inferencing.h>
#include "edge-impulse-sdk/dsp/image/image.hpp"

void Triggle(void *pvParameter);

bool lastSteadyState = LOW;
bool lastFlickerableState = LOW;
uint32_t lastDebounceTime = 0;
uint16_t Debouncetime = 50;
uint32_t buttonPressStartTime = 0;
uint32_t camStartTime = 0;

#define EI_CAMERA_RAW_FRAME_BUFFER_COLS 240
#define EI_CAMERA_RAW_FRAME_BUFFER_ROWS 240
#define EI_CAMERA_FRAME_BYTE_SIZE 3
#define BMP_BUF_SIZE (EI_CAMERA_RAW_FRAME_BUFFER_COLS * EI_CAMERA_RAW_FRAME_BUFFER_ROWS * EI_CAMERA_FRAME_BYTE_SIZE)
#define BBOX_INFO_SIZE 10
#define MAX_BBOX_INFO_LENGTH 64

static uint8_t *bmp_buf;
uint32_t width = 240;
uint32_t height = 240;
bool camState;
char bbox_info[BBOX_INFO_SIZE][MAX_BBOX_INFO_LENGTH];
TaskHandle_t camTaskHandle = NULL; // Task handle variable

// Function prototypes
void ei_prepare_feature(uint8_t *img_buf, signal_t *signal);
int ei_get_feature_callback(size_t offset, size_t length, float *out_ptr);
void ei_use_result(ei_impulse_result_t result);

void cam_detect_task(void *pvParameter);
void Triggle(void *pvParameter);
void setup()
{
  Serial.begin(115200);
  xTaskCreatePinnedToCore(
      Triggle,
      "switchTrig",
      4096,
      nullptr,
      3,
      nullptr,
      0);

  xTaskCreatePinnedToCore(
      cam_detect_task,   // task function
      "cam_detect_task", // name of task
      8192,              // stack size of task
      nullptr,           // parameter of the task
      2,                 // priority of the task
      &camTaskHandle,    // task handle to keep track of created task
      0                  // core to run the task on (0 or 1)
  );
}

void loop()
{
}

void Triggle(void *pvParameter)
{
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);

  while (true)
  {
    bool buttonState = digitalRead(BUTTON_PIN);

    if (buttonState != lastFlickerableState)
    {
      lastDebounceTime = millis();
      lastFlickerableState = buttonState;
      camState = LOW;
    }

    if ((millis() - lastDebounceTime) > Debouncetime)
    {
      if (buttonState == HIGH && camState == LOW)
      {
        if ((millis() - buttonPressStartTime) >= 5000) // Check if 10 seconds have passed since button press
        {
          // if ((millis() - camStartTime >= 10000))
          // {
          digitalWrite(LED_PIN, HIGH); // Turn on LED
          vTaskResume(camTaskHandle);
          if ((millis() - camStartTime) >= 10000)
          {
            camState = HIGH; // Set camState to HIGH after 10 seconds
          }
        }

        else
        {
          camStartTime = millis(); // Set camStartTime when camera task starts
        }
      }
      else
      {
        digitalWrite(LED_PIN, LOW);      // Turn off LED if button is released
        buttonPressStartTime = millis(); // Reset button press start time
      }

      lastSteadyState = buttonState; // Update the last button state
      Serial.print("Button State: ");
      Serial.print(buttonState);
      Serial.print(", Press Start Time: ");
      Serial.print(buttonPressStartTime);
      Serial.print(", Cam Start Time: ");
      Serial.print(camStartTime);
      Serial.print(", Cam State: ");
      Serial.println(camState);
    }
    vTaskDelay(100 / portTICK_PERIOD_MS); // Delay for 100 milliseconds using FreeRTOS delay function
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


    vTaskDelay(1000 / portTICK_PERIOD_MS); // Delay for 2 seconds

    vTaskSuspend(NULL); // Suspend this task after processing

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