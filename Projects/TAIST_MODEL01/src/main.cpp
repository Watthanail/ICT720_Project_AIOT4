#include "watthanai-project-1_inferencing.h"
#include "edge-impulse-sdk/dsp/image/image.hpp"

#include "main.h"
#include "hw_camera.h"
#include "openmvrpc.h"

// constants
#define TAG "main"

/* Constant defines -------------------------------------------------------- */
#define EI_CAMERA_RAW_FRAME_BUFFER_COLS 240
#define EI_CAMERA_RAW_FRAME_BUFFER_ROWS 240
#define EI_CAMERA_FRAME_BYTE_SIZE 3

/* Private variables ------------------------------------------------------- */
static bool debug_nn = false; // Set this to true to see features generated from the raw signal
bool camerasuccess = false;
uint8_t *snapshot_buf; // Points to the output of the capture
bool is_initialised = false;



static uint8_t jpg_buf[20480];
static uint16_t jpg_sz =0;
static bool read_flag =false;
/* Function definitions --------------------------------------------------- */
bool hw_camera_init(bool *success);
uint32_t ei_camera_capture(uint32_t img_width, uint32_t img_height, uint8_t *out_buf);
static int ei_camera_get_data(size_t offset, size_t length, float *out_ptr);



openmv::rpc_scratch_buffer<256> scratch_buffer;
openmv::rpc_callback_buffer<8> callback_buffer;
openmv::rpc_hardware_serial_uart_slave rpc_slave;

size_t jpeg_image_snapshot_callback(void *out_data);
size_t jpeg_image_read_callback(void *out_data);


uint32_t ei_camera_capture(uint32_t img_width, uint32_t img_height, uint8_t *out_buf) {
    uint32_t fb_len;
    bool do_resize = false;
    if (!is_initialised) {
        ei_printf("ERR: Camera is not initialized\r\n");
        return 0;  // Return 0 to indicate failure
    }

    camera_fb_t *fb = esp_camera_fb_get();
    memcpy(snapshot_buf, fb->buf, fb->len);
    fb_len = fb->len;
    // Serial.printf("Image size: %d\n", fb_len);
    // Serial.println();

    if (!fb) {
        ei_printf("Camera capture failed\n");
        return 0;  // Return 0 to indicate failure
    }

    bool converted = fmt2rgb888(fb->buf, fb->len, PIXFORMAT_JPEG, snapshot_buf);

    esp_camera_fb_return(fb);

    if (!converted) {
        ei_printf("Conversion failed\n");
        return 0;  // Return 0 to indicate failure
    }

    if ((img_width != EI_CAMERA_RAW_FRAME_BUFFER_COLS) || (img_height != EI_CAMERA_RAW_FRAME_BUFFER_ROWS)) {
        do_resize = true;
    }

    if (do_resize) {
        ei::image::processing::crop_and_interpolate_rgb888(
            out_buf,
            EI_CAMERA_RAW_FRAME_BUFFER_COLS,
            EI_CAMERA_RAW_FRAME_BUFFER_ROWS,
            out_buf,
            img_width,
            img_height);
    }

    return fb_len;  // Return the image size
}


static int ei_camera_get_data(size_t offset, size_t length, float *out_ptr) {
    // we already have an RGB888 buffer, so recalculate offset into pixel index
    size_t pixel_ix = offset * 3;
    size_t pixels_left = length;
    size_t out_ptr_ix = 0;

    while (pixels_left != 0) {
        out_ptr[out_ptr_ix] = (snapshot_buf[pixel_ix] << 16) + (snapshot_buf[pixel_ix + 1] << 8) + snapshot_buf[pixel_ix + 2];

        // go to the next pixel
        out_ptr_ix++;
        pixel_ix += 3;
        pixels_left--;
    }
    // and done!
    return 0;
}

#if !defined(EI_CLASSIFIER_SENSOR) || EI_CLASSIFIER_SENSOR != EI_CLASSIFIER_SENSOR_CAMERA
#error "Invalid model for the current sensor"
#endif

void setup() {
    // put your setup code here, to run once:
    Serial.begin(115200);
    // comment out the below line to start inference immediately after upload
    is_initialised = hw_camera_init(&camerasuccess);
    rpc_slave.register_callback(F("jpeg_image_snapshot"), jpeg_image_snapshot_callback);
    rpc_slave.register_callback(F("jpeg_image_read"), jpeg_image_read_callback);
    ei_printf("\nStarting continuous inference in 2 seconds...\n");
    ei_sleep(2000);
}

// void loop()
// {
//     // instead of wait_ms, we'll wait on the signal, this allows threads to cancel us...
//     if (ei_sleep(5) != EI_IMPULSE_OK)
//     {
//         return;
//     }
//     snapshot_buf = (uint8_t *)malloc(EI_CAMERA_RAW_FRAME_BUFFER_COLS * EI_CAMERA_RAW_FRAME_BUFFER_ROWS * EI_CAMERA_FRAME_BYTE_SIZE);

//     // check if allocation was successful
//     if (snapshot_buf == nullptr)
//     {
//         ei_printf("ERR: Failed to allocate snapshot buffer!\n");
//         return;
//     }

//     ei::signal_t signal;
//     signal.total_length = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT;
//     signal.get_data = &ei_camera_get_data;

//     if (ei_camera_capture((size_t)EI_CLASSIFIER_INPUT_WIDTH, (size_t)EI_CLASSIFIER_INPUT_HEIGHT, snapshot_buf) == false)
//     {
//         ei_printf("Failed to capture image\r\n");
//         free(snapshot_buf);
//         return;
//     }

//     // Run the classifier
//     ei_impulse_result_t result = {0};

//     EI_IMPULSE_ERROR err = run_classifier(&signal, &result, debug_nn);
//     if (err != EI_IMPULSE_OK)
//     {
//         ei_printf("ERR: Failed to run classifier (%d)\n", err);
//         return;
//     }

//     // print the predictions
//     ei_printf("Predictions (DSP: %d ms., Classification: %d ms., Anomaly: %d ms.): \n",
//               result.timing.dsp, result.timing.classification, result.timing.anomaly);

// #if EI_CLASSIFIER_OBJECT_DETECTION == 1
//     bool bb_found = result.bounding_boxes[0].value > 0;
//     for (size_t ix = 0; ix < result.bounding_boxes_count; ix++)
//     {
//         auto bb = result.bounding_boxes[ix];
//         if (bb.value == 0)
//         {
//             continue;
//         }
//         ei_printf("    %s (%f) [ x: %u, y: %u, width: %u, height: %u ]\n", bb.label, bb.value, bb.x, bb.y, bb.width, bb.height);
//     }
//     if (!bb_found)
//     {
//         ei_printf("    No objects found\n");
//     }
//     // if (read_flag){
//         rpc_slave.put_bytes(jpg_buf,jpg_sz,10000);
//         Serial.printf("Image size02: %d ", jpg_sz);
//     //     read_flag=false;
//     // }
//         rpc_slave.loop();
// #else
//     for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++)
//     {
//         ei_printf("    %s: %.5f\n", result.classification[ix].label,
//                   result.classification[ix].value);
//     }
// #endif

// #if EI_CLASSIFIER_HAS_ANOMALY == 1
//     ei_printf("    anomaly score: %.3f\n", result.anomaly);
// #endif

//     free(snapshot_buf);

// }



void loop() {
    // instead of wait_ms, we'll wait on the signal, this allows threads to cancel us...
    if (ei_sleep(5) != EI_IMPULSE_OK) {
        return;
    }

    snapshot_buf = (uint8_t *)malloc(EI_CAMERA_RAW_FRAME_BUFFER_COLS * EI_CAMERA_RAW_FRAME_BUFFER_ROWS * EI_CAMERA_FRAME_BYTE_SIZE);

    // check if allocation was successful
    if (snapshot_buf == nullptr) {
        ei_printf("ERR: Failed to allocate snapshot buffer!\n");
        return;
    }

    ei::signal_t signal;
    signal.total_length = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT;
    signal.get_data = &ei_camera_get_data;

    uint32_t img_size = ei_camera_capture((size_t)EI_CLASSIFIER_INPUT_WIDTH, (size_t)EI_CLASSIFIER_INPUT_HEIGHT, snapshot_buf);

    if (img_size == 0) {
        ei_printf("Failed to capture image\r\n");
        free(snapshot_buf);
        return;
    }

    // Run the classifier
    ei_impulse_result_t result = {0};

    EI_IMPULSE_ERROR err = run_classifier(&signal, &result, debug_nn);
    if (err != EI_IMPULSE_OK) {
        ei_printf("ERR: Failed to run classifier (%d)\n", err);
        free(snapshot_buf);
        return;
    }

    // print the predictions
    // ei_printf("Predictions (DSP: %d ms., Classification: %d ms., Anomaly: %d ms.): \n",
    //           result.timing.dsp, result.timing.classification, result.timing.anomaly);

    // Send image data if read_flag is set
    if (read_flag) {
        rpc_slave.put_bytes(snapshot_buf, img_size, 10000);
        Serial.printf("Image size02: %d ", img_size);
        Serial.println();
        // read_flag = false;
    }
    rpc_slave.loop();

    free(snapshot_buf);
}



size_t jpeg_image_snapshot_callback(void *out_data) {
    // Capture the image into the snapshot_buf
    uint32_t img_size = ei_camera_capture((size_t)EI_CLASSIFIER_INPUT_WIDTH, (size_t)EI_CLASSIFIER_INPUT_HEIGHT, snapshot_buf);
    if (img_size == 0) {
        ei_printf("Failed to capture image\r\n");
        return 0;
    }

    // Copy the image data into the output buffer
    memcpy(out_data, snapshot_buf, img_size);

    // Return the size of the captured image
    return img_size;
}

size_t jpeg_image_read_callback(void *out_data) {
    read_flag = true;
    return 0;
}






//#############################################################################



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