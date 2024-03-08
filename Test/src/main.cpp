#include "main.h"
#include "hw_camera.h"
#include "openmvrpc.h"

#include "ICT720_Project_AIOT4_inferencing.h"
#include "edge-impulse-sdk/dsp/image/image.hpp"

#include <string> //test

// constants
#define TAG           "main"

#define BUTTON_PIN    0

#define EI_CAMERA_RAW_FRAME_BUFFER_COLS           320
#define EI_CAMERA_RAW_FRAME_BUFFER_ROWS           240
#define EI_CAMERA_FRAME_BYTE_SIZE                 3

// static variables
static uint8_t jpg_buf[20480];
static uint16_t jpg_sz = 0;
static bool read_flag = false;

static bool debug_nn = false; // Set this to true to see e.g. features generated from the raw signal
uint32_t width = 240; 
uint32_t height = 240;
uint8_t *snapshot_buf; //points to the output of the capture

openmv::rpc_scratch_buffer<256> scratch_buffer;
openmv::rpc_callback_buffer<8> callback_buffer;
openmv::rpc_hardware_serial_uart_slave rpc_slave;

// static function declarations
static void print_memory(void);

size_t button_read_callback(void *out_data);
size_t jpeg_image_snapshot_callback(void *out_data);
size_t jpeg_image_read_callback(void *out_data);
size_t detect_obj_callback(void *out_data);
static int ei_camera_get_data(size_t offset, size_t length, float *out_ptr);

// initialize hardware
void setup() {
  Serial.begin(115200);
  hw_camera_init();
  rpc_slave.register_callback(F("button_read"), button_read_callback);
  rpc_slave.register_callback(F("jpeg_image_snapshot"), jpeg_image_snapshot_callback);
  rpc_slave.register_callback(F("jpeg_image_read"), jpeg_image_read_callback);
  rpc_slave.register_callback(F("detect_obj"), detect_obj_callback);
  rpc_slave.begin();
  ESP_LOGI(TAG, "Setup complete");
}

// main loop
void loop() {
  // if (read_flag) {
  //   rpc_slave.put_bytes(jpg_buf, jpg_sz, 10000);
  //   read_flag = false;
  // }
  // rpc_slave.loop();
  Serial.print("start");
  // instead of wait_ms, we'll wait on the signal, this allows threads to cancel us...
  if (ei_sleep(5) != EI_IMPULSE_OK) {
      return;
  }

  // hw_camera_raw_snapshot(jpg_buf, &width, &height);
  // hw_camera_jpg_snapshot(jpg_buf);

  snapshot_buf = (uint8_t*)malloc(EI_CAMERA_RAW_FRAME_BUFFER_COLS * EI_CAMERA_RAW_FRAME_BUFFER_ROWS * EI_CAMERA_FRAME_BYTE_SIZE);

  // check if allocation was successful
  if(snapshot_buf == nullptr) {
      ei_printf("ERR: Failed to allocate snapshot buffer!\n");
      return;
  }

  // camera snapshot in JPEG, then convert to BMP
  hw_camera_raw_snapshot(snapshot_buf, &width, &height);

  ei::signal_t signal;
  signal.total_length = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT;
  signal.get_data = &ei_camera_get_data;


  // Run the classifier
  ei_impulse_result_t result = { 0 };

  EI_IMPULSE_ERROR err = run_classifier(&signal, &result, debug_nn);
  if (err != EI_IMPULSE_OK) {
      ei_printf("ERR: Failed to run classifier (%d)\n", err);
      return;
  }

  // print the predictions
  ei_printf("Predictions (DSP: %d ms., Classification: %d ms., Anomaly: %d ms.): \n",
              result.timing.dsp, result.timing.classification, result.timing.anomaly);

#if EI_CLASSIFIER_OBJECT_DETECTION == 1
    bool bb_found = result.bounding_boxes[0].value > 0;
    for (size_t ix = 0; ix < result.bounding_boxes_count; ix++) {
        auto bb = result.bounding_boxes[ix];
        if (bb.value == 0) {
            continue;
        }
        ei_printf("    %s (%f) [ x: %u, y: %u, width: %u, height: %u ]\n", bb.label, bb.value, bb.x, bb.y, bb.width, bb.height);
    }
    if (!bb_found) {
        ei_printf("    No objects found\n");
    }
#else
    for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
        ei_printf("    %s: %.5f\n", result.classification[ix].label,
                                    result.classification[ix].value);
    }
#endif

#if EI_CLASSIFIER_HAS_ANOMALY == 1
        ei_printf("    anomaly score: %.3f\n", result.anomaly);
#endif

  // end check
  Serial.print("Hee\n");
  free(snapshot_buf);
  // delay(2000);
}

// Print memory information
void print_memory() {
  ESP_LOGI(TAG, "Total heap: %u", ESP.getHeapSize());
  ESP_LOGI(TAG, "Free heap: %u", ESP.getFreeHeap());
  ESP_LOGI(TAG, "Total PSRAM: %u", ESP.getPsramSize());
  ESP_LOGI(TAG, "Free PSRAM: %d", ESP.getFreePsram());
}

// callback for digital_read
size_t button_read_callback(void *out_data) {
  uint8_t state = 1;

  state = !digitalRead(BUTTON_PIN);
  memcpy(out_data, &state, sizeof(state));
  return sizeof(state);
}

// take camera snapshot
size_t jpeg_image_snapshot_callback(void *out_data) {
  jpg_sz = hw_camera_jpg_snapshot(jpg_buf);
  memcpy(out_data, &jpg_sz, sizeof(jpg_sz));
  return sizeof(jpg_sz);
}

static int ei_camera_get_data(size_t offset, size_t length, float *out_ptr) {
    // we already have a RGB888 buffer, so recalculate offset into pixel index
    size_t pixel_ix = offset * 3;
    size_t pixels_left = length;
    size_t out_ptr_ix = 0;

    while (pixels_left != 0) {
        out_ptr[out_ptr_ix] = (snapshot_buf[pixel_ix] << 16) + (snapshot_buf[pixel_ix + 1] << 8) + snapshot_buf[pixel_ix + 2];

        // go to the next pixel
        out_ptr_ix++;
        pixel_ix+=3;
        pixels_left--;
    }
    // and done!
    return 0;
}

// start reading image
size_t jpeg_image_read_callback(void *out_data) {
  read_flag = true;
  return 0;
}

//for send infor
size_t sen_infor(void *out_data) {
  return 0;
  // int
  // uint8_t state = 15;
  // memcpy(out_data, &state, sizeof(state));
  // return sizeof(state);

  // string 1
  // String state = "Fck U";
  // memcpy(out_data, &state, state.length()+1);
  // return sizeof(state);
  
  // string 2
  // String state = "Fck U";
  // const char* state_data = state.c_str();
  // size_t state_size = state.length() + 1; // +1 for the null terminator
  // memcpy(out_data, state_data, state_size);
  // // Return the size of the "state" String object
  // return state_size;

  // string 3
  // char* state = "Fck U";
  // memcpy(out_data, &state, strlen(state)+1);
  // return sizeof(state);
}

// for detect obj
size_t detect_obj_callback(void *out_data) {
  return 0;
}