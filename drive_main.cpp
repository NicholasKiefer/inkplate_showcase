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
void reportStatus(const String& eventName, int version, const String& message);
String urlEncode(const String &str);

// Constants
const char* deviceId = "inkplate-showcase";
const unsigned long updateIntervalMs = 10000;     // 10 seconds for testing, can change to 300000 later
const unsigned long heartbeatIntervalMs = 60000;  // 1 minute heartbeat

// Global variables
unsigned long lastPollMs = 0;
unsigned long lastHeartbeatMs = 0;
String lastRawPayload = "";
int lastDisplayedVersion = -1;

struct ContentPayload {
  int version;
  String mode;
  int size;
  int posX;
  int posY;
  String content;
  String imageUrl;
  bool fullRefresh;
};

void setup_logic() {
}

void logic() {
  if (WiFi.status() != WL_CONNECTED) {
    // WiFi handled in main ino
  }

  if (millis() - lastPollMs >= updateIntervalMs || lastPollMs == 0) {
    lastPollMs = millis();
    pollContent();
  }

  if (millis() - lastHeartbeatMs >= heartbeatIntervalMs || lastHeartbeatMs == 0) {
    lastHeartbeatMs = millis();
    reportStatus("heartbeat", lastDisplayedVersion, "alive");
  }

  delay(50);
}

void pollContent() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("No WiFi, skipping poll.");
    reportStatus("wifi_disconnected", lastDisplayedVersion, "poll skipped");
    return;
  }

  String url = content + "?action=content";
  HTTPClient http;
  http.begin(url);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  int httpCode = http.GET();

  if (httpCode != 200) {
    Serial.println("Content GET failed: " + String(httpCode));
    reportStatus("content_http_error", lastDisplayedVersion, "http=" + String(httpCode));
    http.end();
    return;
  }

  String payload = http.getString();
  http.end();

  if (payload.length() == 0) {
    Serial.println("Empty payload");
    reportStatus("content_empty", lastDisplayedVersion, "empty payload");
    return;
  }

  ContentPayload cp;
  if (!parseJsonPayload(payload, cp)) {
    Serial.println("Invalid JSON payload");
    reportStatus("content_parse_error", lastDisplayedVersion, "invalid json");
    return;
  }

  if (cp.version <= lastDisplayedVersion) {
    Serial.println("No new version. Current=" + String(lastDisplayedVersion) + ", Incoming=" + String(cp.version));
    return;
  }

  reportStatus("new_version_found", cp.version, "updating");

  bool ok = renderPayload(cp);

  if (ok) {
    lastDisplayedVersion = cp.version;
    lastRawPayload = payload;
    reportStatus("display_refresh_done", cp.version, "render success");
  } else {
    reportStatus("display_error", cp.version, "render failed");
  }
}

bool parseJsonPayload(const String& json, ContentPayload& cp) {
  StaticJsonDocument<1024> doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) {
    Serial.println("JSON error: " + String(err.c_str()));
    return false;
  }

  cp.version = doc["version"] | -1;
  cp.mode = doc["mode"] | "text";
  cp.size = doc["size"] | 3;
  cp.posX = doc["pos_x"] | 25;
  cp.posY = doc["pos_y"] | 60;
  cp.content = doc["content"] | "";
  cp.imageUrl = doc["image_url"] | "";
  cp.fullRefresh = doc["full_refresh"] | false;

  return cp.version >= 0;
}

bool renderPayload(const ContentPayload& cp) {
  if (cp.mode == "text") {
    display.clearDisplay();
    display.setTextSize(cp.size);
    display.setCursor(cp.posX, cp.posY);
    display.setTextColor(BLACK);
    display.print(cp.content);
    display.display();
    if (!cp.fullRefresh) {
      display.clearDisplay();  // Clear after display if not full refresh
    }
    return true;
  } else if (cp.mode == "image") {
    display.clearDisplay();
    if (!display.drawImage(cp.imageUrl, 0, 0, true)) {
      Serial.println("Image error. Wrong URL or encoding.");
      return false;
    }
    display.display();
    if (!cp.fullRefresh) {
      display.clearDisplay();
    }
    return true;
  } else {
    Serial.println("Unknown mode: " + cp.mode);
    return false;
  }
}

void reportStatus(const String& eventName, int version, const String& message) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Cannot report, WiFi disconnected.");
    return;
  }

  String url = content +
               "?action=report" +
               "&device=" + urlEncode(deviceId) +
               "&event=" + urlEncode(eventName) +
               "&version=" + String(version) +
               "&rssi=" + String(WiFi.RSSI()) +
               "&message=" + urlEncode(message);

  HTTPClient http;
  http.begin(url);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  int httpCode = http.GET();
  Serial.println("Report [" + eventName + "] => HTTP " + String(httpCode));
  http.end();
}

String urlEncode(const String &str) {
  String encoded = "";
  char c;
  char bufHex[4];

  for (int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += c;
    } else {
      sprintf(bufHex, "%%%02X", c);
      encoded += bufHex;
    }
  }
  return encoded;
}

// Remove old functions that are no longer needed
// Result parseInput(String input);
// void display_text(Result result);
// void display_git(Result result);
