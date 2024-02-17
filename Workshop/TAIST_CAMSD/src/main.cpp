// #include "FS.h"
// #include "SD_MMC.h"
// #include <Arduino.h>
// #include <esp_camera.h>
// #include <esp_log.h>

// #define EEPROM_SIZE 1
// unsigned int pictureCount = 0;

// unsigned int delayTime = 10000;

// #define TAG

// #define CAM_PWDN_PIN -1
// #define CAM_RESET_PIN 18
// #define CAM_XCLK_PIN 14

// #define CAM_SIOD_PIN 4
// #define CAM_SIOC_PIN 5

// #define CAM_Y9_PIN 15
// #define CAM_Y8_PIN 16
// #define CAM_Y7_PIN 17
// #define CAM_Y6_PIN 12
// #define CAM_Y5_PIN 10
// #define CAM_Y4_PIN 8
// #define CAM_Y3_PIN 9
// #define CAM_Y2_PIN 11

// #define CAM_VSYNC_PIN 6
// #define CAM_HREF_PIN 7
// #define CAM_PCLK_PIN 13

// void hw_camera_init()
// {
//   camera_config_t camera_config;

//   // configure hw pins
//   camera_config.ledc_channel = LEDC_CHANNEL_0;
//   camera_config.ledc_timer = LEDC_TIMER_0;
//   camera_config.pin_d0 = CAM_Y2_PIN;
//   camera_config.pin_d1 = CAM_Y3_PIN;
//   camera_config.pin_d2 = CAM_Y4_PIN;
//   camera_config.pin_d3 = CAM_Y5_PIN;
//   camera_config.pin_d4 = CAM_Y6_PIN;
//   camera_config.pin_d5 = CAM_Y7_PIN;
//   camera_config.pin_d6 = CAM_Y8_PIN;
//   camera_config.pin_d7 = CAM_Y9_PIN;
//   camera_config.pin_xclk = CAM_XCLK_PIN;
//   camera_config.pin_pclk = CAM_PCLK_PIN;
//   camera_config.pin_vsync = CAM_VSYNC_PIN;
//   camera_config.pin_href = CAM_HREF_PIN;
//   camera_config.pin_sscb_sda = CAM_SIOD_PIN;
//   camera_config.pin_sscb_scl = CAM_SIOC_PIN;
//   camera_config.pin_pwdn = CAM_PWDN_PIN;
//   camera_config.pin_reset = CAM_RESET_PIN;
//   camera_config.xclk_freq_hz = 20000000;
//   camera_config.pixel_format = PIXFORMAT_JPEG;
//   camera_config.grab_mode = CAMERA_GRAB_LATEST;

//   // configure jpeg settings
//   if (psramFound())
//   {
//     camera_config.frame_size = FRAMESIZE_UXGA;
//     camera_config.jpeg_quality = 10;
//     camera_config.fb_count = 2;
//     camera_config.fb_location = CAMERA_FB_IN_DRAM;
//     ESP_LOGI(TAG, "PSRAM found, using %d frames", camera_config.fb_count);
//   }
//   else
//   {
//     camera_config.frame_size = FRAMESIZE_SVGA;
//     camera_config.jpeg_quality = 12;
//     camera_config.fb_count = 1;
//     camera_config.fb_location = CAMERA_FB_IN_DRAM;
//     ESP_LOGI(TAG, "PSRAM not found, using %d frames", camera_config.fb_count);
//   }
//   // initialize camera
//   // esp_camera_deinit();
//   // delay(100);
//   Serial.printf("Camera init");
//   esp_err_t err = esp_camera_init(&camera_config);
//   if (err != ESP_OK)
//   {
//     // ESP_LOGE(TAG, "Camera init failed with error 0x%x", err);
//     Serial.printf("Camera init failed with errro 0x%x", err);
//     return;
//   }

//   // adjust parameters
//   sensor_t *cam_sensor = esp_camera_sensor_get();
//   cam_sensor->set_framesize(cam_sensor, FRAMESIZE_240X240);
//   cam_sensor->set_brightness(cam_sensor, 0);                 // -2 to 2
//   cam_sensor->set_contrast(cam_sensor, 0);                   // -2 to 2
//   cam_sensor->set_saturation(cam_sensor, 0);                 // -2 to 2
//   cam_sensor->set_special_effect(cam_sensor, 0);             // 0 to 6 (0 - No Effect, 1 - Negative, 2 - Grayscale, 3 - Red Tint, 4 - Green Tint, 5 - Blue Tint, 6 - Sepia)
//   cam_sensor->set_whitebal(cam_sensor, 1);                   // 0 = disable , 1 = enable
//   cam_sensor->set_awb_gain(cam_sensor, 1);                   // 0 = disable , 1 = enable
//   cam_sensor->set_wb_mode(cam_sensor, 0);                    // 0 to 4 - if awb_gain enabled (0 - Auto, 1 - Sunny, 2 - Cloudy, 3 - Office, 4 - Home)
//   cam_sensor->set_exposure_ctrl(cam_sensor, 1);              // 0 = disable , 1 = enable
//   cam_sensor->set_aec2(cam_sensor, 0);                       // 0 = disable , 1 = enable
//   cam_sensor->set_ae_level(cam_sensor, 0);                   // -2 to 2
//   cam_sensor->set_aec_value(cam_sensor, 300);                // 0 to 1200
//   cam_sensor->set_gain_ctrl(cam_sensor, 1);                  // 0 = disable , 1 = enable
//   cam_sensor->set_agc_gain(cam_sensor, 0);                   // 0 to 30
//   cam_sensor->set_gainceiling(cam_sensor, (gainceiling_t)0); // 0 to 6
//   cam_sensor->set_bpc(cam_sensor, 0);                        // 0 = disable , 1 = enable
//   cam_sensor->set_wpc(cam_sensor, 1);                        // 0 = disable , 1 = enable
//   cam_sensor->set_raw_gma(cam_sensor, 1);                    // 0 = disable , 1 = enable
//   cam_sensor->set_lenc(cam_sensor, 1);                       // 0 = disable , 1 = enable
//   cam_sensor->set_hmirror(cam_sensor, 0);                    // 0 = disable , 1 = enable
//   cam_sensor->set_vflip(cam_sensor, 0);                      // 0 = disable , 1 = enable
//   cam_sensor->set_dcw(cam_sensor, 1);                        // 0 = disable , 1 = enable
//   cam_sensor->set_colorbar(cam_sensor, 0);                   // 0 = disable , 1 = enable
//   // if (cam_sensor->id.PID == OV3660_PID){
//   //     cam_sensor->set_vflip(cam_sensor, 1);
//   //     cam_sensor->set_brightness(cam_sensor, 1);
//   //     cam_sensor->set_saturation(cam_sensor, -2);
//   // }
//   // cam_sensor->set_framesize(cam_sensor, FRAMESIZE_QVGA);
// }
// void initMicroSDCard()
// {
//   // Start the MicroSD card
//   Serial.println("Mouting MicorSD Card");
//   if (!SD_MMC.begin())
//   {
//     Serial.println("MicroSD Card Mount Failed");
//     return;
//   }
//   uint8_t cardType = SD_MMC.cardType();

//   if (cardType == CARD_NONE)
//   {
//     Serial.println("NO MicroSD Card found");
//     return;
//   }
// }

// void takeNewPhoto(String path)
// {
//   // Take Picutre with Camer
//   camera_fb_t *fb = NULL;
//   // Setup frame buffer
//   // fb = esp_camera_fb_get();
//   // if (!fb) {
//   //   Serial.println("Camera capture failed");
//   //   return;
//   // }
//   fb = esp_camera_fb_get();
//   if (fb)
//   {
//     Serial.println("Camera capture OK!");
//   }
//   else
//   {
//     Serial.println("Camera capture failed");
//   }
//   fs::FS &fs = SD_MMC;
//   File file = fs.open(path.c_str(), FILE_WRITE);
//   if (!file)
//   {
//     Serial.println("Failed to open file in write mode");
//   }
//   else
//   {
//     file.write(fb->buf, fb->len); // payload (image), payload length
//     Serial.printf("Saved file to path: %s\n", path.c_str());
//   }

//   // Close fthe file
//   file.close();
//   esp_camera_fb_return(fb);
// }

// void setup()
// {
//   // put your setup code here, to run once:
//   Serial.begin(115200);

//   // Initialize the camer
//   Serial.print("Initializing the camera module....");
//   hw_camera_init();
//   Serial.println("Camera Ok!");

//   // Initialize the MicroSD
//   Serial.print("Initializing the MicroSD card module....");
//   initMicroSDCard();
//   Serial.print("Delay Time =");
//   Serial.print(delayTime);
//   Serial.println(" ms");
//   // EEPROM.begin(EEPROM_SIZE);
//   // pictureCount = EEPROM.read(0)+1;

//   // EEPROM.write(0, pictureCount);
//   // EEPROM.commit();
// }

// void loop()
// {
//   // path where new picture will be saved in sd card
//   String path = "/image" + String(pictureCount) + '.jpg';
//   Serial.printf("Picture file name: %s\n", path.c_str());

//   // Take and save photo
//   takeNewPhoto(path);

//   pictureCount++;

//   delay(delayTime);
// }
#include "esp_camera.h"
#include "Arduino.h"
#include "FS.h"                // SD Card ESP32
#include "SD_MMC.h"            // SD Card ESP32
#include "soc/soc.h"           // Disable brownour problems
#include "soc/rtc_cntl_reg.h"  // Disable brownour problems
#include "driver/rtc_io.h"
#include <EEPROM.h>            // read and write from flash memory

// // define the number of bytes you want to access
// #define EEPROM_SIZE 1

// // Pin definition for CAMERA_MODEL_AI_THINKER
#define CAM_PWDN_PIN -1
#define CAM_RESET_PIN 18
#define CAM_XCLK_PIN 14

#define CAM_SIOD_PIN 4
#define CAM_SIOC_PIN 5

#define CAM_Y9_PIN 15
#define CAM_Y8_PIN 16
#define CAM_Y7_PIN 17
#define CAM_Y6_PIN 12
#define CAM_Y5_PIN 10
#define CAM_Y4_PIN 8
#define CAM_Y3_PIN 9
#define CAM_Y2_PIN 11

#define CAM_VSYNC_PIN 6
#define CAM_HREF_PIN 7
#define CAM_PCLK_PIN 13


const char * photoPrefix = "/photo_";
int photoNumber = 0;

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = CAM_Y2_PIN;
  config.pin_d1 = CAM_Y3_PIN;
  config.pin_d2 = CAM_Y4_PIN;
  config.pin_d3 = CAM_Y5_PIN;
  config.pin_d4 = CAM_Y6_PIN;
  config.pin_d5 = CAM_Y7_PIN;
  config.pin_d6 = CAM_Y8_PIN;
  config.pin_d7 = CAM_Y9_PIN;
  config.pin_xclk = CAM_XCLK_PIN;
  config.pin_pclk = CAM_PCLK_PIN;
  config.pin_vsync = CAM_VSYNC_PIN;
  config.pin_href = CAM_HREF_PIN;
  config.pin_sscb_sda = CAM_SIOD_PIN;
  config.pin_sscb_scl = CAM_SIOC_PIN;
  config.pin_pwdn = CAM_PWDN_PIN;
  config.pin_reset = CAM_RESET_PIN;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG; 

  if (psramFound()) {
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  #if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
  #endif

  // camera init
  esp_err_t err = esp_camera_init( & config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  if (s -> id.PID == OV3660_PID) {
    s -> set_vflip(s, 1); // flip it back
    s -> set_brightness(s, 1); // up the brightness just a bit
    s -> set_saturation(s, -2); // lower the saturation
  }
  // drop down frame size for higher initial frame rate
  s -> set_framesize(s, FRAMESIZE_QVGA);

  #if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
  s -> set_vflip(s, 1);
  s -> set_hmirror(s, 1);
  #endif

  Serial.println("Initialising SD card");
  if (!SD_MMC.begin()) {
    Serial.println("Failed to initialise SD card!");
    return;
  }

  uint8_t cardType = SD_MMC.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("SD card slot appears to be empty!");
    return;
  }

}

void loop() {
  camera_fb_t * fb = NULL;
  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return;
  }

  String photoFileName = photoPrefix + String(photoNumber) + ".jpg";
  fs::FS & fs = SD_MMC;
  Serial.printf("Picture file name: %s\n", photoFileName.c_str());

  File file = fs.open(photoFileName.c_str(), FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file in writing mode");
  } else {
    file.write(fb -> buf, fb -> len);
    Serial.printf("Saved file to path: %s\n", photoFileName.c_str());
    ++photoNumber;
  }
  file.close();
  esp_camera_fb_return(fb);

  delay(10000);
}