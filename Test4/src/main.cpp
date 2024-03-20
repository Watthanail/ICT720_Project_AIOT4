// #include <car_detection_inferencing.h>
#include <ICT720_Project_AIOT4_inferencing.h>
#include "edge-impulse-sdk/dsp/image/image.hpp"

#include "main.h"
#include "hw_camera.h"
#include "openmvrpc.h"

// constants
#define TAG           "main"

#define BUTTON_PIN    0

/* Constant defines -------------------------------------------------------- */
#define EI_CAMERA_RAW_FRAME_BUFFER_COLS           320
#define EI_CAMERA_RAW_FRAME_BUFFER_ROWS           240
#define EI_CAMERA_FRAME_BYTE_SIZE                 3     //3 for RGB  | 1 for grayscale 

// static variables
static uint8_t *snapshot_buf;;
static uint8_t jpg_buf[20480];
//static uint8_t *jpg_buf;
static uint16_t jpg_sz = 0;
static bool read_flag = false;
static bool debug_nn = false;
static bool is_initialised = false;


openmv::rpc_scratch_buffer<256> scratch_buffer;
openmv::rpc_callback_buffer<8> callback_buffer;
openmv::rpc_hardware_serial_uart_slave rpc_slave;

/* Function definitions ------------------------------------------------------- */
// bool ei_camera_init(void);
void ei_camera_deinit(void);
bool ei_camera_capture(uint32_t img_width, uint32_t img_height, uint8_t *out_buf) ;
static int ei_camera_get_data(size_t offset, size_t length, float *out_ptr);
void obj_detection_callback(void);//------------------------------------------------------- */

// static function declarations
static void print_memory(void);

size_t button_read_callback(void *out_data); 
size_t jpeg_image_snapshot_callback(void *out_data);
size_t jpeg_image_read_callback(void *out_data);

// initialize hardware
void setup() {
  Serial.begin(115200);

  //both hw_camera_init & ei_camera_init  do the same thing()
  hw_camera_init();
  is_initialised = true;

  // if (ei_camera_init() == false) {
  //   ei_printf("Failed to initialize Camera!\r\n");
  // }
  // else {
  //   ei_printf("Camera initialized\r\n");
  // }

  rpc_slave.register_callback(F("button_read"), button_read_callback);
  rpc_slave.register_callback(F("jpeg_image_snapshot"), jpeg_image_snapshot_callback);
  rpc_slave.register_callback(F("jpeg_image_read"), jpeg_image_read_callback);
  //rpc_slave.register_callback(F("do_obj_detection"), obj_detection_callback);
  rpc_slave.begin();
  ESP_LOGI(TAG, "Setup complete");
}

// main loop
void loop() {
  // if (read_flag) {
  //   rpc_slave.put_bytes(jpg_buf, jpg_sz, 10000);
  //   read_flag = false;
  //   }

  if (ei_sleep(5) != EI_IMPULSE_OK) {
  //The ei_sleep() returns EI_IMPULSE_OK if it successfully slept for the specified duration.
  return;
  }
  obj_detection_callback();
  ei_printf("\nStarting continious inference in 2 seconds...\n");
  // ei_sleep(2000);

  // rpc_slave.loop();
  
}

// bool ei_camera_init(void) {

//     if (is_initialised) return true;

// #if defined(CAMERA_MODEL_ESP_EYE)
//   pinMode(13, INPUT_PULLUP);
//   pinMode(14, INPUT_PULLUP);
// #endif

//     //initialize the camera
//     camera_config_t camera_config;
//     esp_err_t err = esp_camera_init(&camera_config);
//     if (err != ESP_OK) {
//       Serial.printf("Camera init failed with error 0x%x\n", err);
//       return false;
//     }

//     sensor_t * s = esp_camera_sensor_get();
//     // initial sensors are flipped vertically and colors are a bit saturated
//     if (s->id.PID == OV3660_PID) {
//       s->set_vflip(s, 1); // flip it back
//       s->set_brightness(s, 1); // up the brightness just a bit
//       s->set_saturation(s, 0); // lower the saturation
//     }

// #if defined(CAMERA_MODEL_M5STACK_WIDE)
//     s->set_vflip(s, 1);
//     s->set_hmirror(s, 1);
// #elif defined(CAMERA_MODEL_ESP_EYE)
//     s->set_vflip(s, 1);
//     s->set_hmirror(s, 1);
//     s->set_awb_gain(s, 1);
// #endif

//     is_initialised = true;
//     return true;
// }


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
    obj_detection_callback();
    memcpy(out_data, &jpg_sz, sizeof(jpg_sz));
    return sizeof(jpg_sz);
}

// start reading image
size_t jpeg_image_read_callback(void *out_data) {
  read_flag = true;
  return 0;
}



void obj_detection_callback(void){
  
    snapshot_buf = (uint8_t*)malloc(EI_CAMERA_RAW_FRAME_BUFFER_COLS * EI_CAMERA_RAW_FRAME_BUFFER_ROWS * EI_CAMERA_FRAME_BYTE_SIZE);
    //jpg_sz = hw_camera_jpg_snapshot(snapshot_buf);
    // check if allocation was successful
    if(snapshot_buf == nullptr) {
        ei_printf("ERR: Failed to allocate snapshot buffer!\n");
        return;
    }

    ei::signal_t signal;
    signal.total_length = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT;
    signal.get_data = &ei_camera_get_data;    
    //ei_camera_get_data will turn data in snapshot_buf from rgb888 in each pixel into floating point value (3(8bit of R,G,B total 24 bit ) -> combine all into 32bit integer then -> 1 (1 float = 4byte = 32bit) ).


    if (ei_camera_capture((size_t)EI_CLASSIFIER_INPUT_WIDTH, (size_t)EI_CLASSIFIER_INPUT_HEIGHT, snapshot_buf) == false) {
      ei_printf("Failed to capture image\r\n");
      free(snapshot_buf);
      return;
    }

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

    free(snapshot_buf);

}




void ei_camera_deinit(void) {

    //deinitialize the camera
    esp_err_t err = esp_camera_deinit();

    if (err != ESP_OK)
    {
        ei_printf("Camera deinit failed\n");
        return;
    }

    is_initialised = false;
    return;
}



bool ei_camera_capture(uint32_t img_width, uint32_t img_height, uint8_t *out_buf) {
    bool do_resize = false;

    //Initialization Check:-------------------------------------
    if (!is_initialised) {
        ei_printf("ERR: Camera is not initialized\r\n");
        return false;
    }

    //uses the ESP-IDF (Espressif IoT Development Framework) function esp_camera_fb_get() to capture a frame from the camera.
    camera_fb_t *fb = esp_camera_fb_get();

    if (!fb) {
        //If the frame capture fails (returns nullptr), an error message is printed, and the function returns false
        ei_printf("Camera capture failed\n");
        return false;
    }
    //The captured frame is in the JPEG format (PIXFORMAT_JPEG). The function attempts to convert this format to RGB888 (fmt2rgb888 function).
   bool converted = fmt2rgb888(fb->buf, fb->len, PIXFORMAT_JPEG, snapshot_buf);

    //The captured frame buffer is released using esp_camera_fb_return(fb) to free up resources.
   esp_camera_fb_return(fb);

   if(!converted){
        //If the conversion fails, an error message is printed, and the function returns false.
       ei_printf("Conversion failed\n");
       return false;
   }
    //The function checks whether the requested image dimensions (img_width and img_height) are different from the dimensions of the raw camera frame buffer (EI_CAMERA_RAW_FRAME_BUFFER_COLS and EI_CAMERA_RAW_FRAME_BUFFER_ROWS).

    ////if the size of image(W,H) != buffer(W,H) 
    if ((img_width != EI_CAMERA_RAW_FRAME_BUFFER_COLS)
        || (img_height != EI_CAMERA_RAW_FRAME_BUFFER_ROWS)) {
        do_resize = true;
    }

    //then do_resize
    if (do_resize) {
        //If resizing is necessary, it uses ei::image::processing::crop_and_interpolate_rgb888 to resize the image and store it in the provided out_buf.
        ei::image::processing::crop_and_interpolate_rgb888(
        out_buf,
        EI_CAMERA_RAW_FRAME_BUFFER_COLS,
        EI_CAMERA_RAW_FRAME_BUFFER_ROWS,
        out_buf,
        img_width,
        img_height);
    }


    return true;
}

static int ei_camera_get_data(size_t offset, size_t length, float *out_ptr)
{
    // we already have a RGB888 buffer, so recalculate offset into pixel index
    size_t pixel_ix = offset * 3;   //offset = starting pixel
    size_t pixels_left = length;    //number of pixels to process
    //*out_ptr is a pointer to a float array where the converted pixel values will be stored.
    //it's just like a buffer (snapshot_buf) containing RGB888 data ({[R],[G],[B]}...)
    // ()is buffer (snapshot_buf) |  {} is xth pixel  |  [] are the RGB value
    size_t out_ptr_ix = 0;          //out_ptr_ix is an index of the new out_ptr buffer

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