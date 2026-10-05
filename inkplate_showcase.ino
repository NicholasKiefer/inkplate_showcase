#if !defined(ARDUINO_ESP32_DEV) && !defined(ARDUINO_INKPLATE6V2)
#error "Wrong board selection for this example, please select e-radionica Inkplate6 or Soldered Inkplate6 in the boards menu."
#endif

#include "Inkplate.h"    //Include Inkplate library to the sketch
#include <HTTPClient.h>  //Include HTTP library to this sketch
#include <WiFi.h>        //Include ESP32 WiFi library to our sketch
#include "wifistuff.h"  //Include private information like WiFi SSID and password

#include <HTTPUpdate.h>  // ESP32 HTTP Update helper (for OTA updates)
#include <WiFiClientSecure.h>
#include <esp_task_wdt.h>

#include "firmware_config.h"
#include "drive_main.h"  // Include the main drive logic

// Current firmware version. Bump this when releasing a new firmware
#define FIRMWARE_VERSION "1.0.23"
const int HTTP_TIMEOUT_S = 10;
const int TASK_WDT_TIMEOUT_S = 60;

//#define WAKE_BUTTON_PIN 39 // double-check actual pin from schematic or documentation

//Inkplate display(INKPLATE_1BIT);  // Create an object on Inkplate library and also set library into 1 Bit mode (BW)
Inkplate display(INKPLATE_3BIT);
bool staticip = false;
String manifestEtag;
unsigned long startTime;
unsigned long restartTimer;

int cursorline = 0;

int wifiFailCount = 0;


void setup() {
  delay(1000);
  Serial.begin(115200);
  esp_task_wdt_init(TASK_WDT_TIMEOUT_S, true);
  esp_task_wdt_add(NULL);

  setup_display();
  setup_wifi();
  delay(1000);
  analogReadResolution(12);
  startTime = millis();
  restartTimer = millis();
  setup_logic();
}

void loop() {
  esp_task_wdt_reset();

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
  if (millis() - startTime > 900000UL) {
    if (WiFi.status() == WL_CONNECTED) {
      check_for_update();
    } else {
      Serial.println("WiFi disconnected, cannot check for update.");
    }
    startTime = millis();
  }
  esp_task_wdt_reset();
  logic();  // Call the main drive logic from the separate .cpp file
}

void setup_display() {
  display.begin();                     // Init Inkplate library (you should call this function ONLY ONCE)
  display.clearDisplay();              // Clear frame buffer of display
  display.setTextSize(2);              // Set text scaling to two (text will be two times bigger)
  display.setTextColor(BLACK, WHITE);  // Set text color to black and background color to white

  display.clearDisplay();           // Clear everything in frame buffer
  display.setCursor(0, 0);          // Set print cursor to new position
  // E-paper retains the last successful frame across reboot.
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
        // Keep the last successful frame visible during reconnection.
        Serial.println("Connecting to SSID: [" + String(ssids[i]) + "]");
        WiFi.begin(ssids[i], passwords[i]);
        delay(500);
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 10) {
          esp_task_wdt_reset();
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
  
  WiFi.scanDelete();
  esp_task_wdt_reset();
}


// Check a versioned two-line manifest every 15 minutes, using ETag when available.
void check_for_update() {
  Serial.println("Checking for updates...");

  WiFiClientSecure client;
  client.setTimeout(HTTP_TIMEOUT_S);
  client.setHandshakeTimeout(HTTP_TIMEOUT_S);
#ifdef UPDATE_ROOT_CA
  // Use provided root CA for verification if available
  client.setCACert(UPDATE_ROOT_CA);
#else
  // No CA provided — allow insecure connections (skip cert verification)
  client.setInsecure();
#endif

  HTTPClient http;
  if (!http.begin(client, FIRMWARE_MANIFEST_URL)) {
    Serial.println("HTTP begin failed for update check");
    http.end();
    return;
  }
  http.setTimeout(HTTP_TIMEOUT_S * 1000);
  http.setConnectTimeout(HTTP_TIMEOUT_S * 1000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  const char* responseHeaders[] = {"ETag"};
  http.collectHeaders(responseHeaders, 1);
  if (manifestEtag.length()) http.addHeader("If-None-Match", manifestEtag);
  int code = http.GET();
  if (code == HTTP_CODE_NOT_MODIFIED) { http.end(); return; }
  String responseEtag = http.header("ETag");
  if (code != HTTP_CODE_OK) {
    Serial.printf("Update check HTTP error: %d\n", code);
    http.end();
    return;
  }

  if (http.getSize() > 2048) { http.end(); return; }
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

  // Only accept a strictly newer three-part version; stale CDN manifests must
  // never downgrade a device or cause versionless reinstall loops.
  unsigned int remote[3], local[3];
  char extra;
  if (sscanf(remoteVersion.c_str(), "%u.%u.%u%c", &remote[0], &remote[1], &remote[2], &extra) != 3 ||
      sscanf(FIRMWARE_VERSION, "%u.%u.%u", &local[0], &local[1], &local[2]) != 3) {
    Serial.println("Invalid or missing firmware version.");
    return;
  }
  bool newer = false;
  for (int i = 0; i < 3; ++i) {
    if (remote[i] != local[i]) {
      newer = remote[i] > local[i];
      break;
    }
  }
  if (!newer) manifestEtag = responseEtag;
  else manifestEtag = "";  // Retry newer firmware after a failed OTA transfer.
  if (!newer || !binUrl.startsWith("https://") || binUrl.indexOf('\n') >= 0) {
    Serial.println("No valid newer firmware available.");
    return;
  }

  perform_ota_update(binUrl);
}


void perform_ota_update(const String &binUrl) {
  Serial.println("Starting OTA update from: " + binUrl);
  delay(200);

  WiFiClientSecure client;
  client.setTimeout(HTTP_TIMEOUT_S);
  client.setHandshakeTimeout(HTTP_TIMEOUT_S);
#ifdef UPDATE_ROOT_CA
  client.setCACert(UPDATE_ROOT_CA);
#else
  client.setInsecure();
#endif

  esp_task_wdt_reset();
  HTTPUpdate updater(HTTP_TIMEOUT_S * 1000);
  updater.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  updater.onProgress([](int current, int total) { esp_task_wdt_reset(); });
  t_httpUpdate_return ret = updater.update(client, binUrl.c_str());

  switch (ret) {
    case HTTP_UPDATE_FAILED:
      Serial.printf("HTTP_UPDATE_FAILED Error (%d): %s\n", updater.getLastError(), updater.getLastErrorString().c_str());
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