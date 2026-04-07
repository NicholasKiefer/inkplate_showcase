#if !defined(ARDUINO_ESP32_DEV) && !defined(ARDUINO_INKPLATE6V2)
#error "Wrong board selection for this example, please select e-radionica Inkplate6 or Soldered Inkplate6 in the boards menu."
#endif

#include "Inkplate.h"    //Include Inkplate library to the sketch
#include <HTTPClient.h>  //Include HTTP library to this sketch
#include <WiFi.h>        //Include ESP32 WiFi library to our sketch
#include "wifistuff.h"  //Include private information like WiFi SSID and password

#include <HTTPUpdate.h>  // ESP32 HTTP Update helper (for OTA updates)
#include <WiFiClientSecure.h>

#include "drive_main.h"  // Include the main drive logic

// Current firmware version. Bump this when releasing a new firmware
#define FIRMWARE_VERSION "1.0.19"

//#define WAKE_BUTTON_PIN 39 // double-check actual pin from schematic or documentation

//Inkplate display(INKPLATE_1BIT);  // Create an object on Inkplate library and also set library into 1 Bit mode (BW)
Inkplate display(INKPLATE_3BIT);
bool staticip = false;
unsigned long startTime;
unsigned long restartTimer;

int cursorline = 0;

int wifiFailCount = 0;


void setup() {
  delay(1000);
  Serial.begin(115200);

  setup_display();
  setup_wifi();
  delay(1000);
  analogReadResolution(12);
  startTime = millis();
  restartTimer = millis();
  setup_logic();
}

void loop() {
  
  // Check WiFi status and handle failures
  if (WiFi.status() != WL_CONNECTED) {
    wifiFailCount++;
    Serial.println("WiFi disconnected, fail count: " + String(wifiFailCount));
    delay(1000);
    if (wifiFailCount >= 10) {
      Serial.println("WiFi failed 10 times, resetting connection...");
      WiFi.disconnect();
      delay(1000);
      setup_wifi();
      wifiFailCount = 0;
    }
  } else {
    if (wifiFailCount > 0) {
      Serial.println("WiFi reconnected, resetting fail count.");
      wifiFailCount = 0;
    }
  }
  
  // Restart after 1 hour for safety reasons
  if (millis() - restartTimer > 3600000UL) {
    Serial.println("Restarting after 1 hour for safety.");
    ESP.restart();
  }
  
  // After running for x, check for OTA update
  if (millis() - startTime > 60000) {
    if (WiFi.status() == WL_CONNECTED) {
      check_for_update();
    } else {
      Serial.println("WiFi disconnected, cannot check for update.");
    }
    startTime = millis();
  }
  logic();  // Call the main drive logic from the separate .cpp file
}

void setup_display() {
  display.begin();                     // Init Inkplate library (you should call this function ONLY ONCE)
  display.clearDisplay();              // Clear frame buffer of display
  display.setTextSize(2);              // Set text scaling to two (text will be two times bigger)
  display.setTextColor(BLACK, WHITE);  // Set text color to black and background color to white

  display.clearDisplay();           // Clear everything in frame buffer
  display.setCursor(0, 0);          // Set print cursor to new position
  display.display();
}

void setup_wifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  if (staticip) {
    IPAddress local_IP(192,168,1,184);
    IPAddress gateway(192,168,1,1);
    IPAddress subnet(255,255,255,0);
    WiFi.config(local_IP, gateway, subnet);
  }
  
  int n = WiFi.scanNetworks();
  bool connected = false;
  String connectedSSID = "";
  
  for (int i = 0; i < numNetworks && !connected; i++) {
    // Check if this SSID is available
    for (int j = 0; j < n; j++) {
      if (WiFi.SSID(j) == ssids[i]) {
        String message = "Connecting to " + String(ssids[i]) + "...";
        print(message);
        Serial.println("Connecting to SSID: [" + String(ssids[i]) + "]");
        WiFi.begin(ssids[i], passwords[i]);
        delay(500);
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 10) {
          delay(1000);
          attempts++;
          Serial.println("Attempt " + String(attempts) + ", status: " + String(WiFi.status()));
        }
        if (WiFi.status() == WL_CONNECTED) {
          connected = true;
          connectedSSID = ssids[i];
          break;
        }
      }
    }
  }
  
  if (connected) {
    Serial.println("Connected to " + connectedSSID);
  } else {
    Serial.println("Failed to connect to any known network.");
  }
  
  display.clearDisplay();
  display.display();
  display.setCursor(0, 0);
  cursorline = 0;
}


// Check for OTA update. Expects that `manifest` (from wifistuff.h) points to a small
// manifest where the first line is the version string and the second line is the
// direct URL to the .bin file. If the manifest only contains a single line, it
// will be interpreted as the bin URL and the update will be attempted.
void check_for_update() {
  Serial.println("Checking for updates...");

  WiFiClientSecure client;
#ifdef UPDATE_ROOT_CA
  // Use provided root CA for verification if available
  client.setCACert(UPDATE_ROOT_CA);
#else
  // No CA provided — allow insecure connections (skip cert verification)
  client.setInsecure();
#endif

  HTTPClient http;
  if (!http.begin(client, manifest)) {
    Serial.println("HTTP begin failed for update check");
    http.end();
    return;
  }

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("Update check HTTP error: %d\n", code);
    http.end();
    return;
  }

  String payload = http.getString();
  http.end();
  payload.trim();
  if (payload.length() == 0) {
    Serial.println("Empty update manifest.");
    return;
  }

  int nl = payload.indexOf('\n');
  String remoteVersion;
  String binUrl;
  if (nl >= 0) {
    remoteVersion = payload.substring(0, nl);
    binUrl = payload.substring(nl + 1);
  } else {
    remoteVersion = "";
    binUrl = payload;
  }
  remoteVersion.trim();
  binUrl.trim();

  Serial.println("Remote version: " + remoteVersion);
  Serial.println("Bin URL: " + binUrl);

  if (remoteVersion.length() > 0) {
    if (remoteVersion == String(FIRMWARE_VERSION)) {
      Serial.println("Firmware up to date.");
      return;
    } else {
      Serial.println("New firmware available: " + remoteVersion + " (local " + String(FIRMWARE_VERSION) + ")");
    }
  } else {
    Serial.println("No version in manifest; will attempt update from provided URL.");
  }

  if (binUrl.length() == 0) {
    Serial.println("No bin URL provided in manifest.");
    return;
  }

  perform_ota_update(binUrl);
}


void perform_ota_update(const String &binUrl) {
  Serial.println("Starting OTA update from: " + binUrl);
  delay(200);

  WiFiClientSecure client;
#ifdef UPDATE_ROOT_CA
  client.setCACert(UPDATE_ROOT_CA);
#else
  client.setInsecure();
#endif

  t_httpUpdate_return ret = httpUpdate.update(client, binUrl.c_str());

  switch (ret) {
    case HTTP_UPDATE_FAILED:
      Serial.printf("HTTP_UPDATE_FAILED Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
      break;
    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("HTTP_UPDATE_NO_UPDATES");
      break;
    case HTTP_UPDATE_OK:
      Serial.println("HTTP_UPDATE_OK — rebooting...");
      delay(1000);
      ESP.restart();
      break;
  }
}

void print(const String& message) {
  int length = message.length();
  int chunklength = 60;
  for (int i = 0; i<length; i += chunklength) {
    if (cursorline == 28) {
      //display.clearDisplay();
      display.setCursor(0, 0);
      cursorline = 0;
    }
    String chunk = message.substring(i, i + chunklength);
    display.print(chunk);
    display.print("\n");
    cursorline++;
  }
  display.partialUpdate();
}