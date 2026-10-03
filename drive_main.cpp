#include "drive_main.h"
#include <HTTPClient.h>
#include "wifistuff.h"  //Include private information like WiFi SSID and password
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_task_wdt.h>
#include <ArduinoJson.h>

// Forward declarations
struct ContentPayload;
void pollContent();
bool parseJsonPayload(const String& json, ContentPayload& cp);
bool renderPayload(const ContentPayload& cp);
bool downloadAndDrawImage(const String& url);
void reportStatus(const String& message, const ContentPayload& cp);

// Constants
const char* deviceId = "inkplate-showcase";
const unsigned long updateIntervalMs = 10000;     // Keep content updates responsive
const unsigned long heartbeatIntervalMs = 60000;  // 1 minute heartbeat
const int HTTP_TIMEOUT_S = 10;

// Global variables
unsigned long lastPollMs = 0;
unsigned long lastHeartbeatMs = 0;
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

  http.setTimeout(HTTP_TIMEOUT_S * 1000);
  http.setConnectTimeout(HTTP_TIMEOUT_S * 1000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if (!http.begin(client, url)) {
    Serial.println("HTTP begin failed");
    http.end();
    return;
  }
  int httpCode = http.GET();

  if (httpCode != 200) {
    Serial.println("Content GET failed: " + String(httpCode));
    // reportStatus("content_http_error", lastDisplayedVersion, "http=" + String(httpCode));
    http.end();
    return;
  }

  if (http.getSize() > 8192) { http.end(); return; }
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
    reportStatus("alive", cp);
    return;
  }

  // reportStatus("new_version_found", cp.timestamp, "updating");

  esp_task_wdt_reset();
  bool ok = renderPayload(cp);
  esp_task_wdt_reset();

  if (ok) {
    lastDisplayedVersion = cp.timestamp;
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

  if (cp.timestamp.length() == 0) return false;
  if (cp.mode == "text") {
    return cp.content.length() > 0 && cp.size >= 1 && cp.size <= 10 &&
           cp.posX >= 0 && cp.posX < display.width() &&
           cp.posY >= 0 && cp.posY < display.height();
  }
  return cp.mode == "image" && cp.imageUrl.startsWith("https://");
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
    if (!downloadAndDrawImage(cp.imageUrl)) {
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

// Feed PNG decoding only a complete in-memory response. The library's URL
// downloader can hang indefinitely and its stream downloader assumes all bytes
// arrive. A bounded transfer keeps the previous physical frame on failures.
class BufferedImageClient : public WiFiClient {
 public:
  BufferedImageClient(uint8_t* data, size_t size) : data_(data), size_(size) {}
  int available() override { return size_ - offset_; }
  int read() override { return offset_ < size_ ? data_[offset_++] : -1; }
  int read(uint8_t* target, size_t count) override {
    count = min(count, size_ - offset_);
    memcpy(target, data_ + offset_, count);
    offset_ += count;
    return count;
  }
 private:
  uint8_t* data_;
  size_t size_;
  size_t offset_ = 0;
};

bool downloadAndDrawImage(const String& url) {
  WiFiClientSecure client;
  client.setTimeout(HTTP_TIMEOUT_S);
  client.setHandshakeTimeout(HTTP_TIMEOUT_S);
#ifdef UPDATE_ROOT_CA
  client.setCACert(UPDATE_ROOT_CA);
#else
  client.setInsecure();
#endif
  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT_S * 1000);
  http.setConnectTimeout(HTTP_TIMEOUT_S * 1000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if (!http.begin(client, url)) return false;
  int code = http.GET();
  int length = http.getSize();
  // Refuse unknown or excessive lengths instead of decoding truncated data.
  if (code != HTTP_CODE_OK || length <= 0 || length > 1024 * 1024 ||
      ESP.getFreePsram() < (size_t)length * 2 + 65536) {
    http.end();
    return false;
  }
  uint8_t* bytes = (uint8_t*)ps_malloc(length);
  if (!bytes) { http.end(); return false; }
  size_t received = 0;
  unsigned long started = millis();
  WiFiClient* stream = http.getStreamPtr();
  while (received < (size_t)length && millis() - started < 20000UL) {
    int available = stream->available();
    if (available > 0) {
      int count = stream->read(bytes + received,
                              min((size_t)available, (size_t)length - received));
      if (count > 0) received += count;
    } else if (!stream->connected()) {
      break;
    }
    delay(1);
  }
  http.end();
  bool ok = false;
  if (received == (size_t)length) {
    esp_task_wdt_reset();
    if (length >= 24 && memcmp(bytes, "\x89PNG\r\n\x1a\n", 8) == 0) {
      uint32_t width = ((uint32_t)bytes[16] << 24) | ((uint32_t)bytes[17] << 16) |
                       ((uint32_t)bytes[18] << 8) | bytes[19];
      uint32_t height = ((uint32_t)bytes[20] << 24) | ((uint32_t)bytes[21] << 16) |
                        ((uint32_t)bytes[22] << 8) | bytes[23];
      if (width > 0 && width <= (uint32_t)display.width() &&
          height > 0 && height <= (uint32_t)display.height()) {
        BufferedImageClient buffered(bytes, length);
        ok = display.drawPngFromWeb(&buffered, 0, 0, length, true, false);
      }
    } else if (length >= 54 && bytes[0] == 'B' && bytes[1] == 'M') {
      uint32_t offset, width, height, compression, colors;
      uint16_t depth;
      memcpy(&offset, bytes + 10, 4);
      memcpy(&width, bytes + 18, 4);
      memcpy(&height, bytes + 22, 4);
      memcpy(&depth, bytes + 28, 2);
      memcpy(&compression, bytes + 30, 4);
      memcpy(&colors, bytes + 46, 4);
      uint64_t rowBytes = ((uint64_t)width * depth + 31) / 32 * 4;
      uint32_t paletteSize = depth <= 8 ? (colors ? colors : (1U << depth)) : 0;
      if (width > 0 && width <= (uint32_t)display.width() &&
          height > 0 && height <= (uint32_t)display.height() && compression == 0 &&
          (depth == 1 || depth == 4 || depth == 8 || depth == 16 || depth == 24 || depth == 32) &&
          paletteSize <= 256 && offset >= 54 &&
          (paletteSize == 0 || 54 + paletteSize * 4 <= offset) &&
          offset + rowBytes * height <= (uint64_t)length) {
        ok = display.drawBitmapFromBuffer(bytes, 0, 0, true, false);
      }
    } else if (length >= 2 && bytes[0] == 0xff && bytes[1] == 0xd8) {
      ok = display.drawJpegFromBuffer(bytes, length, 0, 0, true, false);
    }
  }
  free(bytes);
  return ok;
}

void reportStatus(const String& message, const ContentPayload& cp) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Cannot report, WiFi disconnected.");
    return;
  }

  if (message == "alive" && lastHeartbeatMs != 0 &&
      millis() - lastHeartbeatMs < heartbeatIntervalMs) return;
  lastHeartbeatMs = millis();
  esp_task_wdt_reset();
  String url = content + "health";

  StaticJsonDocument<256> bodyDoc;
  if (cp.mode == "text") {
    bodyDoc["text"] = cp.content;
    bodyDoc["x"] = cp.posX;
    bodyDoc["y"] = cp.posY;
    bodyDoc["fontSize"] = cp.size;
  }
  else if (cp.mode == "image") {
    bodyDoc["imageURL"] = cp.imageUrl;
  }
  bodyDoc["rssi"] = WiFi.RSSI();
  bodyDoc["message"] = message;

  String body;
  serializeJson(bodyDoc, body);

  WiFiClientSecure client;
  client.setTimeout(HTTP_TIMEOUT_S);
  client.setHandshakeTimeout(HTTP_TIMEOUT_S);
#ifdef UPDATE_ROOT_CA
  client.setCACert(UPDATE_ROOT_CA);
#else
  client.setInsecure();
#endif
  HTTPClient http;
  if (!http.begin(client, url)) return;
  http.setTimeout(HTTP_TIMEOUT_S * 1000);
  http.setConnectTimeout(HTTP_TIMEOUT_S * 1000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.addHeader("Content-Type", "application/json");
  int httpCode = http.POST(body);
  Serial.println("Report => HTTP " + String(httpCode));
  http.end();
}
