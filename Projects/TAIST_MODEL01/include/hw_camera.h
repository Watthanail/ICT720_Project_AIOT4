#ifndef __HW_CAMERA_H__
#define __HW_CAMERA_H__

// include files
#include <Arduino.h>
#include <esp_camera.h>
#include <esp_log.h>


// public function prototypes
void hw_camera_init(void);
uint32_t hw_camera_jpg_snapshot(uint8_t *buffer);
// uint32_t hw_camera_raw_snapshot(uint8_t *buffer, size_t *length);
uint32_t hw_camera_raw_snapshot(uint8_t *buffer);
// uint32_t hw_camera_raw_snapshot02(uint8_t *buffer);
// void hw_camera_raw_snapshot02(uint8_t *buffer, uint32_t *width, uint32_t *height);
uint32_t hw_camera_raw_snapshot02(uint8_t *buffer,uint8_t *rgb_buf ,uint32_t *width, uint32_t *height);
uint32_t hw_camera_jpg_snapshot_square(uint8_t *buffer, int x, int y, int width, int height);
uint32_t ei_camera_get_data(size_t offset, size_t length, float *out_ptr);
#endif // __HW_CAMERA_H__