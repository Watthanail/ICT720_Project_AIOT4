#include "FS.h"
#include "SD.h"
#include "main.h"
#include "hw_camera.h"

// Constants
#define TAG "main"

#define SD_MISO_PIN 40
#define SD_MOSI_PIN 38
#define SD_SCLK_PIN 39
#define SD_CS_PIN 47

unsigned int pictureCount = 0;
unsigned int delayTime = 10000;
static uint8_t jpg_buf[20480];
static uint16_t jpg_sz = 0;
File file;

// Function declarations
void initMicroSDCard();
void takeNewPhoto(String path);

size_t jpeg_image_snapshot_callback(void *out_data);

void sd_test(void)
{
  SPI.begin(SD_SCLK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
  if (!SD.begin(SD_CS_PIN, SPI))
  {
    Serial.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE)
  {
    Serial.println("No SD card attached");
    return;
  }

  Serial.print("SD Card Type: ");

  if (cardType == CARD_MMC)
    Serial.println("MMC");
  else if (cardType == CARD_SD)
    Serial.println("SDSC");
  else if (cardType == CARD_SDHC)
    Serial.println("SDHC");
  else
    Serial.println("UNKNOWN");

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);
  SD.end();
  return;
}

void initMicroSDCard()
{
  // Start the MicroSD card
  Serial.println("Mounting MicroSD Card");

  if (!SD.begin())
  {
    Serial.println("SD Card Mount Failed");
    return;
  }

  uint8_t cardType = SD.cardType();

  if (cardType == CARD_NONE)
  {
    Serial.println("No MicroSD Card found");
    return;
  }
}

void takeNewPhoto(String path)
{
  // Take a picture with the camera
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb)
  {
    Serial.println("Camera capture failed");
    return;
  }

  sd_test();

  file = SD.open(path.c_str(), FILE_WRITE);

  if (!file)
  {
    Serial.println("Failed to open file in write mode");
    esp_camera_fb_return(fb);
    return;
  }

  if (file.write(fb->buf, fb->len) != fb->len)
  {
    Serial.println("Failed to write full buffer to file");
  }

  Serial.printf("Saved file to path: %s\n", path.c_str());

  // Close the file
  file.close();
  esp_camera_fb_return(fb);
}

void setup()
{
  Serial.begin(115200);
  hw_camera_init();
  // sd_test();
  // initMicroSDCard(); // Initialize MicroSD card
}

void loop()
{
  String path = "/image" + String(pictureCount) + ".jpg";
  Serial.printf("Picture file name: %s\n", path.c_str());
  takeNewPhoto(path);
  pictureCount++;

  delay(delayTime);
}