#include "hal/ledc_types.h"
#include "sensor.h"
#include "esp_err.h"
#include "esp_camera.h"
#include "HardwareSerial.h"
#include <Base64.h>


#include "camera.h"


camera_config_t config = {
  .pin_pwdn = PWDN_GPIO_NUM,
  .pin_reset = RESET_GPIO_NUM,
  .pin_xclk = XCLK_GPIO_NUM,

  .pin_sscb_sda = SIOD_GPIO_NUM,
  .pin_sscb_scl = SIOC_GPIO_NUM,

  .pin_d7 = Y9_GPIO_NUM,
  .pin_d6 = Y8_GPIO_NUM,
  .pin_d5 = Y7_GPIO_NUM,
  .pin_d4 = Y6_GPIO_NUM,
  .pin_d3 = Y5_GPIO_NUM,
  .pin_d2 = Y4_GPIO_NUM,
  .pin_d1 = Y3_GPIO_NUM,
  .pin_d0 = Y2_GPIO_NUM,
  .pin_vsync = VSYNC_GPIO_NUM,
  .pin_href = HREF_GPIO_NUM,
  .pin_pclk = PCLK_GPIO_NUM,

  .xclk_freq_hz = 24000000, //20'000'000,

  .ledc_timer = LEDC_TIMER_0,
  .ledc_channel = LEDC_CHANNEL_0,

  .pixel_format = PIXFORMAT_JPEG,
  .frame_size = FRAMESIZE_VGA,
  .jpeg_quality = 10,
  .fb_count = 1
};

void setupCamera()
{
  esp_err_t result = esp_camera_init(&config);
  if (result != ESP_OK)
  {
    Serial.printf("Camera init failed with error 0x%x", result);
  }
  else
  {
    Serial.printf("Camera init succeeded");
  }

  setupSensor();
}

void setupSensor()
{
  sensor_t* sensor = esp_camera_sensor_get();

}

camera_fb_t* getImage()
{
  camera_fb_t* fb = esp_camera_fb_get();
  return fb;  
}

void finishImage(camera_fb_t* fb)
{
  if (fb)
  {
    esp_camera_fb_return(fb);
  }
}