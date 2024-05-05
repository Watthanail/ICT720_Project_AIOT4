// #include <watthanai-project-1_inferencing.h>
// #include "edge-impulse-sdk/dsp/image/image.hpp"
#include "main.h"
#include "hw_camera.h"
#include "openmvrpc.h"


#define EI_CAMERA_RAW_FRAME_BUFFER_COLS           320
#define EI_CAMERA_RAW_FRAME_BUFFER_ROWS           240
#define EI_CAMERA_FRAME_BYTE_SIZE                 3
#include "esp_camera.h"
// static variables
// uint8_t jpg_buf[20480];

// uint8_t *snapshot; // points to the output of the capture
// uint32_t width, height;

struct CameraSnapshotInfo
{
  uint8_t jpg_buf[20480];
  uint8_t *rgb_buf;
  uint16_t jpg_sz;
  uint32_t width;
  uint32_t height;
};
CameraSnapshotInfo info;

// uint32_t height =240;

static uint16_t jpg_sz = 0;
uint16_t outdata[2048];
static bool read_flag = false;

openmv::rpc_scratch_buffer<256> scratch_buffer;
openmv::rpc_callback_buffer<8> callback_buffer;
openmv::rpc_hardware_serial_uart_slave rpc_slave;

void setup()
{
  Serial.begin(115200);
  hw_camera_init();
  // Print each element of outdata

  // rpc_slave.register_callback(F("jpeg_image_snapshot"), jpeg_image_snapshot_callback);

  // rpc_slave.begin();
}

void loop()
{

  // rpc_slave.put_bytes(jpg_buf, jpg_sz, 10000);
  // rpc_slave.loop();
  // info.jpg_sz  = hw_camera_jpg_snapshot(info.jpg_buf,info.rgb_buf);
  info.rgb_buf = (uint8_t*)malloc(EI_CAMERA_RAW_FRAME_BUFFER_COLS * EI_CAMERA_RAW_FRAME_BUFFER_ROWS * EI_CAMERA_FRAME_BYTE_SIZE);
  jpg_sz=hw_camera_raw_snapshot02(info.jpg_buf,info.rgb_buf, &info.width, &info.height);
  printf("Buffer contents (first 16 bytes):\n");
  for (uint32_t i = 0; i < info.jpg_sz; i++)
  {
    printf("%02X ", info.jpg_buf[i]); // Print each byte in hexadecimal format
  }
  printf("\n");

  // printf("Test_case\n");
  Serial.print("Image Width: ");
  Serial.println(info.width);
  Serial.print("Image Height: ");
  Serial.println(info.height);

  delay(5000);
}

// size_t jpeg_image_snapshot_callback(void *out_data) {
//   jpg_sz = hw_camera_jpg_snapshot(jpg_buf);
//   memcpy(out_data, &jpg_sz, sizeof(jpg_sz));
//   return sizeof(jpg_sz);
// }

// size_t jpeg_image_snapshot_callback(void *out_data) {
//     uint8_t *jpg_buf = (uint8_t *)out_data;
//     size_t raw_size;

//     // Call the hw_camera_raw_snapshot function
//     raw_size = hw_camera_raw_snapshot(jpg_buf, &raw_size);

//     // Do other processing or return the size of the JPEG image
//     // (This may depend on your specific use case)

//     // Corrected memcpy call
//     memcpy(out_data, jpg_buf, raw_size);
//     return raw_size;
// }

// size_t jpeg_image_snapshot_callback(void *out_data) {
//     int x = 0;     // starting x-coordinate of the rectangle
//     int y = 0;     // starting y-coordinate of the rectangle
//     int width = 50; // width of the rectangle
//     int height = 50;// height of the rectangle
//     jpg_sz = hw_camera_jpg_snapshot_square(jpg_buf, x, y, width, height);
//     // jpg_sz = hw_camera_jpg_snapshot_square(jpg_buf, 0, 0, 240, 240);
//     memcpy(out_data, &jpg_sz, sizeof(jpg_sz));
//     return sizeof(jpg_sz);
// }
