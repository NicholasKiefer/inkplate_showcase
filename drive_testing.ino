#if !defined(ARDUINO_ESP32_DEV) && !defined(ARDUINO_INKPLATE6V2)
#error "Wrong board selection for this example, please select e-radionica Inkplate6 or Soldered Inkplate6 in the boards menu."
#endif

#include "Inkplate.h"    //Include Inkplate library to the sketch
#include <HTTPClient.h>  //Include HTTP library to this sketch
#include <WiFi.h>        //Include ESP32 WiFi library to our sketch
#include "wifistuff.h"

//#define WAKE_BUTTON_PIN 39 // double-check actual pin from schematic or documentation

unsigned long ms = millis();
unsigned long readMillis = 0;
String displaytext;

//Inkplate display(INKPLATE_1BIT);  // Create an object on Inkplate library and also set library into 1 Bit mode (BW)
Inkplate display(INKPLATE_3BIT);
bool staticip = false;

struct Result {
  String firstLine;
  String remainingText;
};

int cursorline = 0;
int updateMs = 30000;


void setup() {
  delay(1000);
  Serial.begin(115200);

  setup_display();
  setup_wifi();
  delay(1000);
  analogReadResolution(12);
  //pinMode(GPIO_NUM_36, INPUT_PULLUP); // Set wake-up button as input
}

void loop() {
  // check time
  if (!(millis() - readMillis > updateMs || readMillis == 0 || detect_button())) {
    return;
  }

  // check internet connection
  readMillis = millis();
  if (WiFi.status() != WL_CONNECTED) {
    // keep counter, wait 1 min and try again
    Serial.println("WiFi disconnected.");
    return;
  }
  // check delta
  // update
  HTTPClient http;
  http.getStream().setNoDelay(true);
  http.getStream().setTimeout(1);
  http.begin(download);
  int responsecode = http.GET();
  int bodyLen = http.getSize();
  String encodedString = http.getString();
  String responsecodestr = String(responsecode);
  if (responsecode != 200) {
    String message = "http response code: " + String(responsecode);
    Serial.println(message);
    return;
    }
  if (encodedString == displaytext) {
    Serial.println("encoded string is already displayed.");
    return;
    }


  displaytext = encodedString;
  Result res = parseInput(encodedString);


  if (res.firstLine == String("text")) {
    display_text(res);
    }

  if (res.firstLine == String("web")) {
    display_git(res);
    }
  return;
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
  //scan_wifi();  // first a scan

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  if (staticip) {
    IPAddress local_IP(192,168,1,184);
    IPAddress gateway(192,168,1,1);
    IPAddress subnet(255,255,255,0);
    WiFi.config(local_IP, gateway, subnet);
  }
  
  String message = "Connecting to " + String(ssid) + "...";
  print(message);
  WiFi.begin(ssid, pass);  // Try to connect to WiFi network
  Serial.println("SSID: [" + String(ssid) + "]");
  Serial.println("PASS: [" + String(pass) + "]");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    String message = "status: " + String(WiFi.status());
    Serial.println(message);
  }
  print("Connected.");
  Serial.println("Connected.");
  display.clearDisplay();
  display.display();
  display.setCursor(0, 0);
  cursorline = 0;
}


void scan_wifi() {
  print("Scanning WiFis...");
  Serial.println("Scanning WiFis...");
  int n = WiFi.scanNetworks();
  print("Scan complete.");
  Serial.println("Scan complete.");
  if (n == 0) {
    print("No networks found.");
    Serial.println("No networks found.");
  } else {
    String message = "found " + String(n) + " networks";
    print(message);
    for (int i = 0; i < n; ++i) {
      message = String(i + 1) + ": " + WiFi.SSID(i) + " (RSSI: " + String(WiFi.RSSI(i)) + " dBm)";
      Serial.println(String(i + 1) + ": " + WiFi.SSID(i) + " (RSSI: " + String(WiFi.RSSI(i)) + " dBm)");
      print(message);
    }
  }
}

bool detect_button() {
  int rawValue = analogRead(36);
  float voltage = (rawValue / 4095.0) * 3.3;
  // Threshold (tune based on your hardware — usually < 0.5V when pressed)
  bool buttonPressed = voltage < 0.5;
  if (buttonPressed) {
    Serial.println("Wake button pressed! updating...");
    // trigger refresh instead of waiting
    // debounce button
    delay(500);
    return true;
  }
  return false;
}