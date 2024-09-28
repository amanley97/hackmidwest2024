
#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>

#define flashPin 4
#define DELAY 500

// Your WiFi credentials
const char* ssid = "WiFi Name";
const char* password = "WiFI Password";

// Dropbox Access Token
const char* dropboxAccessToken = "INSERT YOUR TOKEN HERE";

// Dropbox API upload URL
const char* dropboxUploadUrl = "https://content.dropboxapi.com/2/files/upload";

// Path to upload image in Dropbox
const char* dropboxPath = "/esp32cam_image.jpg"; // Change this to the desired file path

void setup() {
  Serial.begin(115200);
  connectWiFi();

  pinMode(flashPin, OUTPUT);
  digitalWrite(flashPin, LOW);

  // Initialize the camera
  if (!initCamera()) {
    Serial.println("Camera initialization failed");
    return;
  }

  digitalWrite(flashPin, HIGH);
  delay(DELAY);

  // Capture the image
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Failed to capture image");
    return;
  }

  digitalWrite(flashPin, LOW);

  // Upload image to Dropbox
  if (uploadToDropbox(fb->buf, fb->len)) {
    Serial.println("Image uploaded successfully!");
  } else {
    Serial.println("Image upload failed.");
  }

  // Release the frame buffer
  esp_camera_fb_return(fb);
}

void loop() {
  // Nothing to do here
}

void connectWiFi() {
  Serial.print("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(DELAY);
    Serial.print(".");
  }
  Serial.println("WiFi connected");
}

bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = 5;
  config.pin_d1 = 18;
  config.pin_d2 = 19;
  config.pin_d3 = 21;
  config.pin_d4 = 36;
  config.pin_d5 = 39;
  config.pin_d6 = 34;
  config.pin_d7 = 35;
  config.pin_xclk = 0;
  config.pin_pclk = 22;
  config.pin_vsync = 25;
  config.pin_href = 23;
  config.pin_sccb_sda = 26;
  config.pin_sccb_scl = 27;
  config.pin_pwdn = 32;
  config.pin_reset = -1;
  config.pin_xclk = 0;
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

  // Camera initialization
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return false;
  }

  // Get the camera sensor object to adjust settings
  sensor_t* s = esp_camera_sensor_get();
  if (s != NULL) {
    s->set_whitebal(s, 1);      // Enable white balance
    s->set_awb_gain(s, 1);      // Enable auto white balance gain
    s->set_exposure_ctrl(s, 1); // Enable exposure control
    s->set_aec_value(s, 300);   // Adjust auto exposure value (300 is an example, experiment with different values)
  }

  return true;
}

bool uploadToDropbox(uint8_t* imageData, size_t imageSize) {
  HTTPClient http;
  
  // Set up Dropbox API headers
  String dropboxArg = "{\"path\": \"" + String(dropboxPath) + "\",\"mode\": \"overwrite\",\"autorename\": false,\"mute\": false}";

  http.begin(dropboxUploadUrl);
  http.addHeader("Authorization", "Bearer " + String(dropboxAccessToken));
  http.addHeader("Content-Type", "application/octet-stream");
  http.addHeader("Dropbox-API-Arg", dropboxArg);

  // Send the image data
  int httpResponseCode = http.POST((uint8_t*)imageData, imageSize);

  if (httpResponseCode > 0) {
    Serial.printf("HTTP Response code: %d\n", httpResponseCode);
    String response = http.getString();
    Serial.println("Response: " + response);
    http.end();
    return httpResponseCode == 200;
  } else {
    Serial.printf("Error occurred during HTTP request: %s\n", http.errorToString(httpResponseCode).c_str());
    http.end();
    return false;
  }
}
