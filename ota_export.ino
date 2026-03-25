// File: OTA_update.ino
// Requires: Inkplate library, WiFi, HTTPClient, HTTPUpdate
// Also requires a "wifistuff.h" providing at least:
//   const char* ssid;
//   const char* pass;
//   const char* download;            // (existing) URL to the Google Apps Script / Drive doc used to drive display
// Optionally provide a manifest URL by defining UPDATE_MANIFEST_URL in wifistuff.h
// If UPDATE_MANIFEST_URL is not defined, the code will use `download` as the update manifest URL.
// Manifest format (plain text):
//   line1: version string (e.g. 1.0.1)
//   line2: direct URL to .bin file (HTTP/HTTPS)
// The .bin can be hosted on a static server, GitHub raw/Release, or direct download link from a cloud service
// (ensure the link does not require authentication).
//
// This sketch:
//  - Reads "download" (Google Apps Script / Drive doc) and displays its content using two modes:
//      first line "text"  => remaining content parsed for fonts/position and text payload
//      first line "web"   => second line a URL to an image to draw with display.drawImage()
//  - Checks a manifest URL for OTA .bin and performs HTTP OTA when a newer version is available.

#if !defined(ARDUINO_ESP32_DEV) && !defined(ARDUINO_INKPLATE6V2)
#error "Wrong board selection for this example, please select e-radionica Inkplate6 or Soldered Inkplate6 in the boards menu."
#endif

#include "Inkplate.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <HTTPUpdate.h>
#include <WiFiClientSecure.h>
#include "wifistuff.h" // must provide ssid, pass, download (and optionally define UPDATE_MANIFEST_URL)

#define FIRMWARE_VERSION "1.0.1"

#ifndef UPDATE_MANIFEST_URL
static const char* manifestUrl = download; // fallback: reuse download as manifest
#else
static const char* manifestUrl = UPDATE_MANIFEST_URL;
#endif

Inkplate display(INKPLATE_3BIT);

struct Result {
    String firstLine;
    String remainingText;
};

unsigned long readMillis = 0;
String displaytext;
int cursorline = 0;
int updateMs = 30000;

void setup() {
    delay(1000);
    Serial.begin(115200);
    setup_display();
    setup_wifi();
    delay(1000);
    analogReadResolution(12);
}

void loop() {
    if (!(millis() - readMillis > updateMs || readMillis == 0)) {
        return;
    }
    readMillis = millis();

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi disconnected.");
        return;
    }

    // 1) fetch and render the drive/doc content that controls the display
    fetch_and_render_display();

    // 2) check manifest and perform OTA if a new firmware is available
    check_for_update();
}

void setup_display() {
    display.begin();
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(BLACK, WHITE);
    display.clearDisplay();
    display.setCursor(0, 0);
    display.display();
}

void setup_wifi() {
    Serial.println("Scanning WiFis...");
    int n = WiFi.scanNetworks();
    Serial.println("Scan complete. Found " + String(n) + " networks.");
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(ssid, pass);
    Serial.println("Connecting to " + String(ssid));
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        if (millis() - start > 20000) {
            Serial.println("\nStill connecting...");
            start = millis();
        }
    }
    Serial.println("\nConnected. IP: " + WiFi.localIP().toString());
    display.clearDisplay();
    display.setCursor(0, 0);
    cursorline = 0;
}

// Fetch the Google Apps Script/Drive doc content and render according to its control lines.
void fetch_and_render_display() {
    WiFiClientSecure client;
#ifdef UPDATE_ROOT_CA
    client.setCACert(UPDATE_ROOT_CA);
#else
    client.setInsecure();
#endif

    HTTPClient http;
    if (!http.begin(client, download)) {
        Serial.println("Display fetch HTTP begin failed");
        http.end();
        return;
    }
    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.println("Display fetch HTTP error: " + String(code));
        http.end();
        return;
    }
    String payload = http.getString();
    http.end();
    payload.trim();
    if (payload.length() == 0) {
        Serial.println("Empty display document.");
        return;
    }
    if (payload == displaytext) {
        Serial.println("Content unchanged.");
        return;
    }
    displaytext = payload;
    Result res = parseInput(payload);

    if (res.firstLine == String("text")) {
        display_text(res);
    } else if (res.firstLine == String("web")) {
        display_git(res);
    } else {
        Serial.println("Unknown display mode: " + res.firstLine);
    }
}

// Parse first line (mode) and remaining text
Result parseInput(String input) {
    int newlineIndex = input.indexOf('\n');
    String firstLine;
    String remainingText;
    if (newlineIndex < 0) {
        firstLine = input;
        remainingText = "";
    } else {
        firstLine = input.substring(0, newlineIndex);
        remainingText = input.substring(newlineIndex + 1);
    }
    firstLine.trim();
    remainingText.trim();
    return {firstLine, remainingText};
}

// "text" mode: second line contains size and position, rest is text
void display_text(Result result) {
    String remainingText = result.remainingText;
    int secondLineIndex = remainingText.indexOf("\n");
    if (secondLineIndex < 0) {
        Serial.println("display_text: malformed payload (no second line)");
        return;
    }
    String secondLine = remainingText.substring(0, secondLineIndex);
    String textToDisplay = remainingText.substring(secondLineIndex + 1);

    secondLine.trim();
    textToDisplay.trim();

    int firstSpaceIndex = secondLine.indexOf(' ');
    if (firstSpaceIndex < 0) {
        Serial.println("display_text: malformed size/position line");
        return;
    }
    int size = secondLine.substring(0, firstSpaceIndex).toInt();
    String posPart = secondLine.substring(firstSpaceIndex + 1);
    posPart.trim();
    int commaIndex = posPart.indexOf(',');
    if (commaIndex < 0) {
        Serial.println("display_text: malformed position");
        return;
    }
    int pos_x = posPart.substring(0, commaIndex).toInt();
    int pos_y = posPart.substring(commaIndex + 1).toInt();

    display.clearDisplay();
    display.setTextSize(size);
    display.setCursor(pos_x, pos_y);
    display.setTextColor(BLACK);
    display.print(textToDisplay);
    display.display();
    // keep display contents until the next update; do not immediately clear
    Serial.println("Displayed text.");
}

// "web" mode: first remaining line is image URL
void display_git(Result result) {
    int secondLineIndex = result.remainingText.indexOf("\n");
    String downloadlink;
    if (secondLineIndex < 0) {
        downloadlink = result.remainingText;
    } else {
        downloadlink = result.remainingText.substring(0, secondLineIndex);
    }
    downloadlink.trim();
    Serial.println("Downloading image: " + downloadlink);
    display.clearDisplay();
    if (!display.drawImage(downloadlink, 0, 0, true)) {
        Serial.println("image error. Wrong url or wrong image encoding.");
        return;
    }
    display.display();
    Serial.println("Showing image from download.");
}

// OTA: check manifestUrl for version/bin, then perform update if needed.
void check_for_update() {
    Serial.println("Checking for OTA updates...");

    WiFiClientSecure client;
#ifdef UPDATE_ROOT_CA
    client.setCACert(UPDATE_ROOT_CA);
#else
    client.setInsecure();
#endif

    HTTPClient http;
    if (!http.begin(client, manifestUrl)) {
        Serial.println("HTTP begin failed for update check");
        http.end();
        return;
    }
    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.println("Update check HTTP error: " + String(code));
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
    WiFiClient client;
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