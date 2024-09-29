#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>

#define PWDN_GPIO_NUM    32
#define RESET_GPIO_NUM   -1
#define XCLK_GPIO_NUM    0
#define SIOD_GPIO_NUM    26
#define SIOC_GPIO_NUM    27

#define Y9_GPIO_NUM      35
#define Y8_GPIO_NUM      34
#define Y7_GPIO_NUM      39
#define Y6_GPIO_NUM      36
#define Y5_GPIO_NUM      21
#define Y4_GPIO_NUM      19
#define Y3_GPIO_NUM      18
#define Y2_GPIO_NUM      5
#define VSYNC_GPIO_NUM   25
#define HREF_GPIO_NUM    23
#define PCLK_GPIO_NUM    22


const char* ssid = "ssid";           // Your WiFi SSID
const char* password = "password";   // Your WiFi password
const char* dropboxAccessToken = "Insert token access";  // Your Dropbox access token

#define FLASH_GPIO_NUM 4  // Pin for the flash (GPIO 4 is often used)

void startCameraServer();
void uploadToDropbox(const uint8_t* imageBuffer, size_t imageSize);

void setup() {
  Serial.begin(115200);

  // Initialize flash LED pin
  pinMode(FLASH_GPIO_NUM, OUTPUT);
  digitalWrite(FLASH_GPIO_NUM, LOW); // Turn off flash initially
  
  // Start the camera
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG; 

  // Frame settings
  // Frame settings
  // Frame settings
  if (psramFound()) {
      config.frame_size = FRAMESIZE_SVGA;  // Use SVGA for lower resolution
      config.jpeg_quality = 20;             // Set higher quality for smaller size
      config.fb_count = 2;
  } else {
      config.frame_size = FRAMESIZE_QVGA;   // Use QVGA for lower resolution
      config.jpeg_quality = 20;              // Set higher quality for smaller size
      config.fb_count = 1;
  }

  // Camera initialization
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  Serial.print("Camera ready! Use this URL: http://");
  Serial.print(WiFi.localIP());
  Serial.println("/capture");

  // Start the web server
  startCameraServer();
}

void loop() {
  delay(10000);  // Do nothing, let the server handle requests
}

void startCameraServer() {
  WiFiServer server(80);  // Start a server on port 80
  server.begin();

  while (true) {
    WiFiClient client = server.available();
    if (!client) {
      continue;
    }

    // Wait for client to send a request
    Serial.println("New client connected");
    while (!client.available()) {
      delay(1);
    }

    // Read the client's request
    String request = client.readStringUntil('\r');
    client.flush();
    
    // Check if request is for the capture page
    if (request.indexOf("/capture") != -1) {
      digitalWrite(FLASH_GPIO_NUM, HIGH); // Turn on flash
      delay(100); // Wait for the flash to turn on for a brief moment

      camera_fb_t * fb = esp_camera_fb_get();
      if (!fb) {
        Serial.println("Camera capture failed");
        client.println("HTTP/1.1 500 Internal Server Error");
        client.println("Content-Type: text/html");
        client.println();
        client.println("Capture failed!");
        digitalWrite(FLASH_GPIO_NUM, LOW); // Turn off flash
        return;
      }

      // Upload the captured image to Dropbox
      uploadToDropbox(fb->buf, fb->len);

      // Respond back to the client
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: text/html");
      client.println();
      client.println("Image captured and uploaded to Dropbox.");

      esp_camera_fb_return(fb);
      digitalWrite(FLASH_GPIO_NUM, LOW); // Turn off flash
    } else {
      // If not "/capture", return 404
      client.println("HTTP/1.1 404 Not Found");
      client.println("Content-Type: text/html");
      client.println();
      client.println("Page not found");
    }

    delay(1);
    Serial.println("Client disconnected");
    client.stop();
  }
}

void uploadToDropbox(const uint8_t* imageBuffer, size_t imageSize) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    // Prepare the request
    String filePath = "/photo.jpg"; // Change this to your desired file path in Dropbox
    http.begin("https://content.dropboxapi.com/2/files/upload");
    http.addHeader("Authorization", "Bearer " + String(dropboxAccessToken));
    http.addHeader("Dropbox-API-Arg", "{\"path\": \"" + filePath + "\",\"mode\": \"overwrite\",\"autorename\": true,\"mute\": false}");
    http.addHeader("Content-Type", "application/octet-stream");

    // Log the size of the payload
    Serial.printf("Image Size: %d bytes\n", imageSize);

    // Cast to uint8_t* before sending the request
    int httpResponseCode = http.sendRequest("POST", (uint8_t*)imageBuffer, imageSize);

    // Check response
    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.printf("Upload Response: %s\n", response.c_str());
    } else {
      Serial.printf("Error uploading to Dropbox: %s\n", http.errorToString(httpResponseCode).c_str());
      Serial.printf("HTTP response code: %d\n", httpResponseCode); // Log the response code
    }

    http.end(); // Close the connection
  } else {
    Serial.println("WiFi not connected. Unable to upload.");
  }
}


