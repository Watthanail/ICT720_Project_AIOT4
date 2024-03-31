#include "main.h"
#include "hw_camera.h"
#include <Workshop01_inferencing.h>
#include "edge-impulse-sdk/dsp/image/image.hpp"
#include "openmvrpc.h"

// Constants
#define TAG "main"
#define BTN_PIN 0


#define EI_CAMERA_RAW_FRAME_BUFFER_COLS 240
#define EI_CAMERA_RAW_FRAME_BUFFER_ROWS 240
#define EI_CAMERA_FRAME_BYTE_SIZE 3
#define BMP_BUF_SIZE (EI_CAMERA_RAW_FRAME_BUFFER_COLS * EI_CAMERA_RAW_FRAME_BUFFER_ROWS * EI_CAMERA_FRAME_BYTE_SIZE)
#define BBOX_INFO_SIZE 10
#define MAX_BBOX_INFO_LENGTH 64

static uint8_t *bmp_buf;
uint32_t width = 240;
uint32_t height = 240;
static uint8_t jpg_buf[20480];
static uint16_t jpg_sz = 0;
static bool read_flag = false;

char bbox_info[BBOX_INFO_SIZE][MAX_BBOX_INFO_LENGTH]; // Fixed-size char arrays for bounding box info

openmv::rpc_scratch_buffer<256> scratch_buffer;
openmv::rpc_callback_buffer<8> callback_buffer;
openmv::rpc_hardware_serial_uart_slave rpc_slave;

// Function prototypes
void ei_prepare_feature(uint8_t *img_buf, signal_t *signal);
int ei_get_feature_callback(size_t offset, size_t length, float *out_ptr);
void ei_use_result(ei_impulse_result_t result);

size_t jpeg_image_snapshot_callback(void *out_data);
size_t jpeg_image_read_callback(void *out_data);
size_t sent_information_callback(void *out_data);

// Initialize hardware
void setup()
{
  Serial.begin(115200);

  hw_camera_init();
  bmp_buf = (uint8_t *)ps_malloc(BMP_BUF_SIZE);
  rpc_slave.register_callback(F("jpeg_image_snapshot"), jpeg_image_snapshot_callback);
  rpc_slave.register_callback(F("jpeg_image_read"), jpeg_image_read_callback);
  rpc_slave.register_callback(F("sent_information"), sent_information_callback);
  rpc_slave.begin();
}

// Main loop
void loop()
{
  if (read_flag)
  {
    rpc_slave.put_bytes(jpg_buf, jpg_sz, 10000);
    // Serial.println("Taking snapshot...");
    // hw_camera_raw_snapshot(bmp_buf, &width, &height);
    // ei::signal_t signal;
    // ei_prepare_feature(bmp_buf, &signal);
    // ei_impulse_result_t result = {0};
    // bool debug_nn = false;
    // run_classifier(&signal, &result, debug_nn);
    // ei_use_result(result);
    read_flag = false;
  }

  Serial.println("Taking snapshot...");
  hw_camera_raw_snapshot(bmp_buf, &width, &height);
  ei::signal_t signal;
  ei_prepare_feature(bmp_buf, &signal);
  ei_impulse_result_t result = {0};
  bool debug_nn = false;
  run_classifier(&signal, &result, debug_nn);
  ei_use_result(result);
  rpc_slave.loop();

}

// Callback functions
size_t jpeg_image_snapshot_callback(void *out_data)
{
  jpg_sz = hw_camera_jpg_snapshot(jpg_buf);
  memcpy(out_data, &jpg_sz, sizeof(jpg_sz));
  return sizeof(jpg_sz);
}

size_t jpeg_image_read_callback(void *out_data)
{
  read_flag = true;
  return 0;
}

// size_t sent_information_callback(void *out_data)
// {
//   size_t totalSize = 0;
//   for (int i = 0; i < BBOX_INFO_SIZE; i++)
//   {
//     size_t len = strlen(bbox_info[i]);
//     // Copy only up to the maximum length
//     size_t copy_len = len < MAX_BBOX_INFO_LENGTH ? len : MAX_BBOX_INFO_LENGTH - 1;
//     memcpy((char *)out_data + totalSize, bbox_info[i], copy_len);
//     totalSize += copy_len;
//     // Add null terminator
//     *((char *)out_data + totalSize) = '\0';
//     totalSize++; // Increment totalSize to account for null terminator
//   }
//   return totalSize;
// }

size_t sent_information_callback(void *out_data)
{
  size_t totalSize = 0;

  // Check if the first element is "No objects found"
  if (strcmp(bbox_info[0], "No objects found") == 0) {
    // If it is, clear the bbox_info array
    for (int i = 0; i < BBOX_INFO_SIZE; i++) {
      memset(bbox_info[i], 0, MAX_BBOX_INFO_LENGTH);
    }
  }

  // Populate the out_data with bbox_info
  for (int i = 0; i < BBOX_INFO_SIZE; i++)
  {
    size_t len = strlen(bbox_info[i]);
    // Copy only up to the maximum length
    size_t copy_len = len < MAX_BBOX_INFO_LENGTH ? len : MAX_BBOX_INFO_LENGTH - 1;
    memcpy((char *)out_data + totalSize, bbox_info[i], copy_len);
    totalSize += copy_len;
    // Add null terminator
    *((char *)out_data + totalSize) = '\0';
    totalSize++; // Increment totalSize to account for null terminator
  }
  return totalSize;
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

// #######################################################

// #include "main.h"
// #include "hw_camera.h"
// #include "openmvrpc.h"

// // constants
// #define TAG           "main"

// #define BUTTON_PIN    0

// // static variables
// static uint8_t jpg_buf[20480];
// static uint16_t jpg_sz = 0;
// static bool read_flag = false;

// openmv::rpc_scratch_buffer<256> scratch_buffer;
// openmv::rpc_callback_buffer<8> callback_buffer;
// openmv::rpc_hardware_serial_uart_slave rpc_slave;

// // static function declarations
// static void print_memory(void);

// size_t button_read_callback(void *out_data);
// size_t jpeg_image_snapshot_callback(void *out_data);
// size_t jpeg_image_read_callback(void *out_data);

// // initialize hardware
// void setup() {
//   Serial.begin(115200);
//   hw_camera_init();
//   rpc_slave.register_callback(F("button_read"), button_read_callback);
//   rpc_slave.register_callback(F("jpeg_image_snapshot"), jpeg_image_snapshot_callback);
//   rpc_slave.register_callback(F("jpeg_image_read"), jpeg_image_read_callback);
//   rpc_slave.begin();
//   ESP_LOGI(TAG, "Setup complete");
// }

// // main loop
// void loop() {
//   if (read_flag) {
//     rpc_slave.put_bytes(jpg_buf, jpg_sz, 10000);
//     read_flag = false;
//   }
//   rpc_slave.loop();
// }

// // Print memory information
// void print_memory() {
//   ESP_LOGI(TAG, "Total heap: %u", ESP.getHeapSize());
//   ESP_LOGI(TAG, "Free heap: %u", ESP.getFreeHeap());
//   ESP_LOGI(TAG, "Total PSRAM: %u", ESP.getPsramSize());
//   ESP_LOGI(TAG, "Free PSRAM: %d", ESP.getFreePsram());
// }

// // callback for digital_read
// size_t button_read_callback(void *out_data) {
//   uint8_t state = 1;

//   state = !digitalRead(BUTTON_PIN);
//   memcpy(out_data, &state, sizeof(state));
//   return sizeof(state);
// }

// // take camera snapshot
// size_t jpeg_image_snapshot_callback(void *out_data) {
//   jpg_sz = hw_camera_jpg_snapshot(jpg_buf);
//   memcpy(out_data, &jpg_sz, sizeof(jpg_sz));
//   return sizeof(jpg_sz);
// }

// // start reading image
// size_t jpeg_image_read_callback(void *out_data) {
//   read_flag = true;
//   return 0;
// }
