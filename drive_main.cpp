#include "drive_main.h"
#include <HTTPClient.h>
#include "wifistuff.h"  //Include private information like WiFi SSID and password

// Global variables
int updateMs = 10000;
String displaytext = "";
unsigned long readMillis = 0;

void setup_logic() {
}

int logic() {
  // check time
  if (!(millis() - readMillis > updateMs || readMillis == 0)) {
    return 0;
  }

  // check internet connection
  readMillis = millis();
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected.");
    return 1;
  }
  // check delta
  // update
  HTTPClient http;
  http.getStream().setNoDelay(true);
  http.getStream().setTimeout(1);
  http.begin(content);
  int responsecode = http.GET();
  int bodyLen = http.getSize();
  String encodedString = http.getString();
  String responsecodestr = String(responsecode);
  if (responsecode != 200) {
    String message = "http response code: " + String(responsecode);
    Serial.println(message);
    return 1;
    }
  if (encodedString == displaytext) {
    Serial.println("encoded string is already displayed.");
    return 2;
    }


  displaytext = encodedString;
  Result res = parseInput(encodedString);


  if (res.firstLine == String("text")) {
    display_text(res);
    }

  if (res.firstLine == String("web")) {
    display_git(res);
    }
  return 2;
}

Result parseInput(String input) {
  int newlineIndex = input.indexOf('\n');
  String firstLine = input.substring(0, newlineIndex);
  String remainingText = input.substring(newlineIndex + 1);
  return {firstLine, remainingText};
}

void display_text(Result result) {
  String remainingText = result.remainingText;
  int secondLineIndex = remainingText.indexOf("\n");
  String secondLine = remainingText.substring(0, secondLineIndex);
  String textToDisplay = remainingText.substring(secondLineIndex + 1);
  
  // second row is like: >3 25,25<    first int is text size, rest is position
  int firstSpaceIndex = secondLine.indexOf(' ');
  int size = secondLine.substring(0, firstSpaceIndex).toInt();

  // Parse the position values (pos_x and pos_y)
  String posPart = secondLine.substring(firstSpaceIndex + 1);
  int commaIndex = posPart.indexOf(',');

  int pos_x = posPart.substring(0, commaIndex).toInt();
  int pos_y = posPart.substring(commaIndex + 1).toInt();
    // print text to display given size and posx posy
    display.clearDisplay();
    display.setTextSize(size);
    display.setCursor(pos_x, pos_y);
    display.setTextColor(BLACK);
    display.print(textToDisplay);
    display.display();
    display.clearDisplay();
  return;
}

void display_git(Result result) {
    // display github bmp
    int secondLineIndex = result.remainingText.indexOf("\n");
    String downloadlink = result.remainingText.substring(0, secondLineIndex);
    String message = "download link: " + downloadlink;
    Serial.println(message);
    display.clearDisplay();
    if (!display.drawImage(downloadlink, 0, 0, true)) {
      Serial.println("image error. Wrong url or wrong image encoding.");
      return;
    }
    display.display();
    Serial.println("showing image from download.");
    return;
}
