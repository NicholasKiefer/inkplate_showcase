
#if !defined(ARDUINO_ESP32_DEV) && !defined(ARDUINO_INKPLATE6V2)
#error "Wrong board selection for this example, please select e-radionica Inkplate6 or Soldered Inkplate6 in the boards menu."
#endif

#include "Inkplate.h"    //Include Inkplate library to the sketch
#include <HTTPClient.h>  //Include HTTP library to this sketch
#include <WiFi.h>        //Include ESP32 WiFi library to our sketch
#include "base64.hpp"
#include "wifistuff.h"
#include "DSTManager.h"

unsigned long ms = millis();
unsigned long readMillis = 0;
String displaytext;

Inkplate display(INKPLATE_1BIT);  // Create an object on Inkplate library and also set library into 1 Bit mode (BW)
int statusBarHeight = 12;

struct Result {
  int size;
  int pos_x;
  int pos_y;
  String text;
};

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000);  // NTP client


void setup() {
  display.begin();                     // Init Inkplate library (you should call this function ONLY ONCE)
  display.clearDisplay();              // Clear frame buffer of display
  display.setTextSize(2);              // Set text scaling to two (text will be two times bigger)
  display.setTextColor(BLACK, WHITE);  // Set text color to black and background color to white

  display.clearDisplay();           // Clear everything in frame buffer
  display.setCursor(0, 0);          // Set print cursor to new position
  display.print("Connecting to ");  // Print the name of WiFi network
  display.print(ssid);
  WiFi.begin(ssid, pass);  // Try to connect to WiFi network
  WiFi.setAutoReconnect(true);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);  // While it is connecting to network, display dot every second, just to know that Inkplate is
                  // alive.
    display.print('.');
    display.partialUpdate();
  }
  display.print("connected");  // If it's connected, notify user
  display.partialUpdate();
  display.clearDisplay();
  display.setCursor(0, 0);
  delay(1);
  display.display();
  //testsite();
  //statusbar();
  timeClient.begin();
}

void loop() {
  if (!(millis() - readMillis > 10000 || readMillis == 0)) {
    return;
  }
  readMillis = millis();
  // If wifi is down, do nothing else and print status off
  if (WiFi.status() != WL_CONNECTED) {
    // should reconnect to network? dont care todo
    // statusbar("off", "off");
    return;
  }
  HTTPClient http;
  http.begin(download);
  int responsecode = http.GET();
  int bodyLen = http.getSize();
  String encodedString = http.getString();
  String responsecodestr = String(responsecode);
  if (responsecode != 200) {
    //statusbar("on", responsecodestr);
    return;
  }
  if (encodedString == displaytext) {
    return;
  }
  displaytext = encodedString;
  Result res = parseInput(encodedString);
  // print text to display given size and posx posy
  display.clearDisplay();
  display.setTextSize(res.size);
  display.setCursor(res.pos_x, res.pos_y);
  display.print(res.text);
  display.display();
  display.clearDisplay();
}

Result parseInput(String input) {
  int newlineIndex = input.indexOf('\n');
  String firstLine = input.substring(0, newlineIndex);
  String remainingText = input.substring(newlineIndex + 1);

  // Parse the first int (size)
  int firstSpaceIndex = firstLine.indexOf(' ');
  int size = firstLine.substring(0, firstSpaceIndex).toInt();

  // Parse the position values (pos_x and pos_y)
  String posPart = firstLine.substring(firstSpaceIndex + 1);
  int commaIndex = posPart.indexOf(',');

  int pos_x = posPart.substring(0, commaIndex).toInt();
  int pos_y = posPart.substring(commaIndex + 1).toInt();

  return { size, pos_x, pos_y, remainingText };
}

void statusbar(String WifiStatus, String SiteStatus) {
  timeClient.update();
  updateDST(timeClient);  // Call the DST adjustment function
  String formattedTime = timeClient.getFormattedTime();
  String Status = formattedTime + "";
  for (int i = 0; i < 30; i++) {
    Status += " ";
  }
  Status += "WiFi: " + WifiStatus + " Site: " + SiteStatus;
  display.setCursor(0, 0);
  display.setTextSize(11);
  display.print(Status);
  display.partialUpdate();
  display.clearDisplay();
}