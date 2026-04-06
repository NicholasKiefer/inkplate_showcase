#include "drive_main.h"
#include <HTTPClient.h>
#include "wifistuff.h"  //Include private information like WiFi SSID and password
#include <WiFi.h>
#include <ArduinoJson.h>

// Forward declarations
struct ContentPayload;
void pollContent();
bool parseJsonPayload(const String& json, ContentPayload& cp);
bool renderPayload(const ContentPayload& cp);
void reportStatus(const String& message, const ContentPayload& cp);

// Constants
const char* deviceId = "inkplate-showcase";
const unsigned long updateIntervalMs = 10000;     // 10 seconds for testing, can change to 300000 later
const unsigned long heartbeatIntervalMs = 60000;  // 1 minute heartbeat

// Global variables
unsigned long lastPollMs = 0;
unsigned long lastHeartbeatMs = 0;
String lastRawPayload = "";
String lastDisplayedVersion = "";

struct ContentPayload {
  String timestamp;
  String mode;
  int size;
  int posX;
  int posY;
  String content;
  String imageUrl;
};

void setup_logic() {
}

void logic() {
  if (WiFi.status() != WL_CONNECTED) {
    // WiFi handled in main ino
    return;
  }

  if (millis() - lastPollMs >= updateIntervalMs || lastPollMs == 0) {
    lastPollMs = millis();
    pollContent();
  }

  // if (millis() - lastHeartbeatMs >= heartbeatIntervalMs || lastHeartbeatMs == 0) {
  //   lastHeartbeatMs = millis();
  //   // reportStatus("alive");
  // }

  delay(50);
}

void pollContent() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("No WiFi, skipping poll.");
    //reportStatus("wifi_disconnected", lastDisplayedVersion, "poll skipped");
    return;
  }

  String url = content + "current";
  HTTPClient http;
  http.begin(url);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  int httpCode = http.GET();

  if (httpCode != 200) {
    Serial.println("Content GET failed: " + String(httpCode));
    // reportStatus("content_http_error", lastDisplayedVersion, "http=" + String(httpCode));
    http.end();
    return;
  }

  String payload = http.getString();
  http.end();

  if (payload.length() == 0) {
    Serial.println("Empty payload");
    // reportStatus("content_empty", lastDisplayedVersion, "empty payload");
    return;
  }

  ContentPayload cp;
  if (!parseJsonPayload(payload, cp)) {
    Serial.println("Invalid JSON payload");
    // reportStatus("content_parse_error", lastDisplayedVersion, "invalid json");
    return;
  }

  if (cp.timestamp == lastDisplayedVersion) {
    Serial.println("No new version. Current=" + lastDisplayedVersion + ", Incoming=" + cp.timestamp);
    return;
  }

  // reportStatus("new_version_found", cp.timestamp, "updating");

  bool ok = renderPayload(cp);

  if (ok) {
    lastDisplayedVersion = cp.timestamp;
    lastRawPayload = payload;
    reportStatus("render success", cp);
  } else {
    reportStatus("render failed", cp);
  }
}

bool parseJsonPayload(const String& json, ContentPayload& cp) {
  StaticJsonDocument<1024> doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) {
    Serial.println("JSON error: " + String(err.c_str()));
    return false;
  }

  cp.timestamp = doc["timestamp"] | "";
  cp.mode = doc["mode"] | "text";
  cp.size = doc["text"]["fontSize"] | 3;
  cp.posX = doc["text"]["x"] | 25;
  cp.posY = doc["text"]["y"] | 60;
  cp.content = doc["text"]["content"] | "";
  cp.imageUrl = doc["image"] | "";

  return cp.timestamp != "";
}

bool renderPayload(const ContentPayload& cp) {
  if (cp.mode == "text") {
    display.clearDisplay();
    display.setTextSize(cp.size);
    display.setTextColor(BLACK);
    display.setCursor((int16_t) cp.posX, (int16_t) cp.posY);
    display.print(cp.content);
    display.display();
    return true;
  } else if (cp.mode == "image") {
    display.clearDisplay();
    if (!display.drawImage(cp.imageUrl, 0, 0, true)) {
      Serial.println("Image error. Wrong URL or encoding.");
      return false;
    }
    display.display();
    return true;
  } else {
    Serial.println("Unknown mode: " + cp.mode);
    return false;
  }
}

void reportStatus(const String& message, const ContentPayload& cp) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Cannot report, WiFi disconnected.");
    return;
  }

  String endpoint = content;
  String url = content + "health";

  StaticJsonDocument<256> bodyDoc;
  if (cp.mode == "text") {
    bodyDoc["text"] = cp.content;
    bodyDoc["x"] = cp.posX;
    bodyDoc["y"] = cp.posY;
  }
  else if (cp.mode == "image") {
    bodyDoc["imageURL"] = cp.imageUrl;
  }
  bodyDoc["rssi"] = WiFi.RSSI();
  bodyDoc["message"] = message;

  String body;
  serializeJson(bodyDoc, body);

  HTTPClient http;
  http.begin(url);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.addHeader("Content-Type", "application/json");
  int httpCode = http.POST(body);
  Serial.println("Report => HTTP " + String(httpCode) + " body=" + body);
  http.end();
}
