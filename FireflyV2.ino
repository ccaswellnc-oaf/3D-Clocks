//Program for getting date, time and weather for a location
//presenting them on a "Cheap Yellow Display"
//Requires an API key from OpenWeatherMap.org
//The cobfiguration file is kept on a microSD card
//The clock face also has a scroll area to display quotes from a list on the microSD card


//The accompanying 3D print files are meant to be printed with a clear filament
//LEDs shine through the layered silhoutte made from the TV show Firefly
//Code for using LEDs within the clock for back lighting is mostly commented out because I couldn't get the power right


#include <WiFi.h>
#include <SPI.h>
#include <SD.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <TFT_eSPI.h>
#include <HTTPClient.h>
#include <ezTime.h>
#include <FastLED.h>

uint32_t MYGREEN = TFT_GREEN;
uint32_t MYBLUE = TFT_OLIVE;
uint32_t MYRED = TFT_RED;
uint32_t MYYELLOW = TFT_YELLOW;
uint32_t MYBLACK = TFT_BLACK;
uint32_t MYWHITE = TFT_WHITE;


// Define hardware connections
#define LED_PIN 21  // Adjust as needed
#define NUM_LEDS 14  // Number of LEDs in the string
#define SD_CS 5     // SD Card Chip Select Pin
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240


#define SCROLL_DELAY 50  // Adjust scrolling speed (lower = faster)
#define REPEAT_COUNT 5  // Number of times to scroll before choosing a new line

CRGB leds[NUM_LEDS];

String Qlines[50];

int qcount = 0;
bool quoteOn = false;


// Initialize peripherals

TFT_eSPI tft = TFT_eSPI();
AsyncWebServer server(80);

Timezone myTZ;

int lastWck = 0;
int wInterval = 900000;  // update weather every 15 minutes

// Variables to store configuration
String ssid, password, apiKey, cityName, timeZone;
float latitude, longitude;
int red, green, blue, bright;
bool wifiNeedsRestart = false;
bool tzUpdated = true;
String lastMin = "AA";
bool updatedCity = true;
bool newLEDs = true;

void initializeQs(){
  for (int q=0; q < 50; q++){
  Qlines[q] = "";
}
}

// Read setup.txt from SD card
void loadConfig() {
  if (!SD.begin(SD_CS)) {
    Serial.println("Failed to initialize SD card!");
    return;
  }


  File file = SD.open("/setup.txt");
  if (!file) {
    Serial.println("Failed to open setup.txt");
    return;
  }

  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();

    if (line.startsWith("SSID=")) ssid = line.substring(5);
    else if (line.startsWith("PASSWORD=")) password = line.substring(9);
    else if (line.startsWith("APIKEY=")) apiKey = line.substring(7);
    else if (line.startsWith("LAT=")) latitude = line.substring(4).toFloat();
    else if (line.startsWith("LON=")) longitude = line.substring(4).toFloat();
    else if (line.startsWith("TIMEZONE=")) timeZone = line.substring(9);
    else if (line.startsWith("CITY=")) cityName = line.substring(5);
    else if (line.startsWith("RED=")) red = constrain(line.substring(4).toInt(), 0, 255);
    else if (line.startsWith("GREEN=")) green = constrain(line.substring(6).toInt(), 0, 255);
    else if (line.startsWith("BLUE=")) blue = constrain(line.substring(5).toInt(), 0, 255);
    else if (line.startsWith("BRIGHTNESS=")) bright = constrain(line.substring(11).toInt(), 0, 255);
  }
  file.close();

  File Qfile = SD.open("/quotes.txt");
  if (!Qfile) {
    Serial.println("Failed to open quotes.txt");
    return;
  }
  quoteOn = true;
  while (Qfile.available()) {
    Qlines[qcount] = Qfile.readStringUntil('\n');
    if (!Qlines[qcount].startsWith("//")  &&  qcount < 50){
    Qlines[qcount].trim();
    qcount++;
    }
  }
  file.close();
}

void printConfigValues() {
  Serial.println("Configuration:");
  Serial.println(ssid);
  Serial.println(password);
  Serial.println(apiKey);
  Serial.println(latitude);
  Serial.println(longitude);
  Serial.println(timeZone);
  Serial.println(cityName);
  Serial.println(red);
  Serial.println(green);
  Serial.println(blue);
  Serial.println(bright);
}

// Save configuration to SD card
void saveConfig() {
  File file = SD.open("/setup.txt", FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open setup.txt for writing");
    return;
  }
  Serial.printf("SSID=%s\nPASSWORD=%s\nAPIKEY=%s\nLAT=%.6f\nLON=%.6f\nTIMEZONE=%s\nCITY=%s\nRED=%d\nGREEN=%d\nBLUE=%d\nBRIGHTNESS=%d\n",
                ssid.c_str(), password.c_str(), apiKey.c_str(), latitude, longitude, timeZone.c_str(), cityName.c_str(), red, green, blue, bright);
  file.printf("SSID=%s\nPASSWORD=%s\nAPIKEY=%s\nLAT=%.6f\nLON=%.6f\nTIMEZONE=%s\nCITY=%s\nRED=%d\nGREEN=%d\nBLUE=%d\nBRIGHTNESS=%d\n",
              ssid.c_str(), password.c_str(), apiKey.c_str(), latitude, longitude, timeZone.c_str(), cityName.c_str(), red, green, blue, bright);
  file.close();
}



// Fetch weather data
void fetchWeather() {
  String wsString;
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "http://api.openweathermap.org/data/2.5/weather?lat=" + String(latitude) + "&lon=" + String(longitude) + "&units=imperial&appid=" + apiKey;
    http.begin(url);
    Serial.println("Checking for weather: " + url);
    int httpCode = http.GET();
    if (httpCode > 0) {
      String payload = http.getString();
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, payload);
      //float temperature = doc["main"]["temp"].as<float>();
      String temperature = doc["main"]["temp"];
      int dotIndex = temperature.indexOf('.');
      if (dotIndex != -1) {                                // If a decimal exists
        temperature = temperature.substring(0, dotIndex);  // Keep only the part before the decimal
      }
      String weather = doc["weather"][0]["main"];
      // tft.fillScreen(MYWHITE);
      tft.setTextColor(MYWHITE, MYBLUE);
      tft.fillRect(0, 150, 320, 20, MYBLUE);
      //String cityString = "Weather for " + cityName;
      tft.setTextSize(2);
      int16_t cWidth = tft.textWidth(cityName);
      if (cWidth > tft.width()) {
        tft.setTextSize(1);
        cWidth = tft.textWidth(cityName);
      }
      int16_t csPos = (tft.width() - cWidth) / 2;
      tft.drawString(cityName, csPos, 160);
      tft.setTextSize(2);
      tft.fillRect(0, 180, 320, 20, MYBLUE);
      if (temperature == "null" || weather == "null") {
        tft.setTextSize(1);
        tft.setTextColor(MYRED, MYBLUE);
        wsString = "Error retrieving weather data";
      } else {
        wsString = temperature + "F " + weather;
      }
      int16_t wsWidth = tft.textWidth(wsString);
      int16_t wsPos = (tft.width() - wsWidth) / 2;
      tft.drawString(wsString, wsPos, 185);
    } else {
      tft.setTextColor(MYWHITE, MYBLUE);
      tft.fillRect(0, 170, 320, 20, MYBLUE);
      tft.setTextSize(1);
      String errorSt = "HTTP request failed: " + String(httpCode);
      tft.drawString(errorSt, 10, 185);
    }
    http.end();
  }
}

// Put the clock on the screen
void displayTime(String clockStr) {
  int16_t textWidth, x;
  tft.fillRect(0, 0, 320, 140, MYBLUE);
  tft.setTextColor(MYWHITE, MYBLUE);
  tft.setTextSize(6);
  //Serial.println("tzName is " + myTZ.getTimezoneName());
  //String clockStr = myTZ.dateTime("g:i A");
  textWidth = tft.textWidth(clockStr);  // Get width of text
  x = (tft.width() - textWidth) / 2;    // Center X position
  tft.drawString(clockStr, x, 60);
  tft.setTextSize(2);
  String dteStr = myTZ.dateTime("D, M jS, Y");
  textWidth = tft.textWidth(dteStr);  // Get width of text
  x = (tft.width() - textWidth) / 2;  // Center X position
  tft.drawString(dteStr, x, 25);
}

//Put the IP Address in the bottom corner
void displayIP(String tzN) {
  IPAddress thisIP = WiFi.localIP();
  tft.fillRect(0, 223, 320, 14, MYBLUE);
  tft.setTextColor(MYWHITE, MYBLUE);
  tft.setTextSize(1);
  tft.drawString(tzN, 10, 226);
  String ipStr = "IP:" + thisIP.toString();
  int16_t ipWidth = tft.textWidth(ipStr);  // Get width of text
  //Serial.println("Width of " + ipStr + "is " + ipWidth);
  int16_t x = (tft.width() - ipWidth) - 6;  // Right X position
  tft.drawString(ipStr, x, 226);
}

// Handle client requests
void setupWebServer() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    String html = "<html><body>"
                  "<h1>ESP32 Configuration</h1>"
                  "<form action='/update' method='POST'>"
                  "SSID: <input type='text' name='ssid' value='"
                  + ssid + "'><br>"
                           "Password: <input type='text' name='password'><br>"
                           "API Key: <input type='text' name='apikey' value='"
                  + apiKey + "'><br>"
                             "Latitude: <input type='text' name='latitude' value='"
                  + String(latitude) + "'><br>"
                                       "Longitude: <input type='text' name='longitude' value='"
                  + String(longitude) + "'><br><br>"
                  + "TimeZone Value must be in a format from https://en.wikipedia.org/wiki/List_of_tz_database_time_zones" + "<br>"
                                                                                                                             "Time Zone: <input type='text' name='timezone' value='"
                  + timeZone + "'><br>"
                               "City: <input type='text' name='city' value='"
                  + cityName + "'><br>Color values should be between 0 - 255<br>"
                               "Red: <input type='number' name='red' min='0' max='255' value='"
                  + String(red) + "'><br>"
                                  "Green: <input type='number' name='green' min='0' max='255' value='"
                  + String(green) + "'><br>"
                                    "Blue: <input type='number' name='blue' min='0' max='255' value='"
                  + String(blue) + "'><br>Brightness value 0 - 100<br>"
                                    "Brightness: <input type='number' name='bright' min='0' max='100' value='"
                  + String(bright) + "'><br>"
                                   "<input type='submit' value='Update'>"
                                   "</form></body></html>";
    request->send(200, "text/html", html);
  });

  server.on("/update", HTTP_POST, [](AsyncWebServerRequest* request) {
    if (!request->hasParam("ssid", true) || request->getParam("ssid", true)->value().isEmpty() || !request->hasParam("password", true) || request->getParam("password", true)->value().isEmpty() || !request->hasParam("apikey", true) || request->getParam("apikey", true)->value().isEmpty() || !request->hasParam("latitude", true) || request->getParam("latitude", true)->value().isEmpty() || !request->hasParam("longitude", true) || request->getParam("longitude", true)->value().isEmpty() || !request->hasParam("timezone", true) || request->getParam("timezone", true)->value().isEmpty() || !request->hasParam("city", true) || request->getParam("city", true)->value().isEmpty() || !request->hasParam("red", true) || request->getParam("red", true)->value().isEmpty() || !request->hasParam("green", true) || request->getParam("green", true)->value().isEmpty() || !request->hasParam("blue", true) || request->getParam("blue", true)->value().isEmpty()|| !request->hasParam("bright", true) || request->getParam("bright", true)->value().isEmpty()) {
      request->send(400, "text/html", "<h1>Error: All fields must be filled!</h1><a href='/'>Back</a>");
      return;
    }

    String newSSID = request->getParam("ssid", true)->value();
    String newPassword = request->getParam("password", true)->value();
    apiKey = request->getParam("apikey", true)->value();
    float newLatitude = request->getParam("latitude", true)->value().toFloat();
    float newLongitude = request->getParam("longitude", true)->value().toFloat();
    String newtimeZone = request->getParam("timezone", true)->value();
    String newcityName = request->getParam("city", true)->value();
    int newRed = constrain(request->getParam("red", true)->value().toInt(), 0, 255);
    int newGreen = constrain(request->getParam("green", true)->value().toInt(), 0, 255);
    int newBlue = constrain(request->getParam("blue", true)->value().toInt(), 0, 255);
    int newBright = constrain(request->getParam("bright", true)->value().toInt(), 0, 100);
    if (newBright > 100){
      newBright = 100;
    }


    if (newSSID != ssid || newPassword != password) {
      ssid = newSSID;
      password = newPassword;
      wifiNeedsRestart = true;
    }
    if (newLatitude != latitude || newLongitude != longitude) {
      latitude = newLatitude;
      longitude = newLongitude;
      updatedCity = true;
    }
    if (newtimeZone != timeZone) {
      timeZone = newtimeZone;
      tzUpdated = true;
    }
    if (newcityName != cityName) {
      cityName = newcityName;
      updatedCity = true;
    }
    if (newRed != red || newGreen != green || newBlue != blue || newBright != bright) {
      red = newRed;
      green = newGreen;
      blue = newBlue;
      newLEDs = true;
    }
    saveConfig();
    request->send(200, "text/html", "<h1>Updated!</h1><br>It may take a few seconds for the display to update.<br><br><a href='/'>Back</a>");
  });
  server.begin();
}


void displayNetworkInfo() {
  IPAddress ip = WiFi.localIP();
  IPAddress gateway = WiFi.gatewayIP();
  IPAddress subnet = WiFi.subnetMask();

  tft.fillScreen(MYBLUE);
  tft.setCursor(10, 10);
  tft.println("WiFi Connected!");
  tft.setCursor(10, 50);
  tft.print("IP: ");
  tft.println(ip);
  tft.setCursor(10, 80);
  tft.print("Gateway: ");
  tft.println(gateway);
  tft.setCursor(10, 110);
  tft.print("Subnet: ");
  tft.println(subnet);
  delay(1000);
  tft.fillScreen(MYBLUE);



  Serial.println("WiFi Connected:");
  Serial.print("IP Address: ");
  Serial.println(ip);
  Serial.print("Gateway: ");
  Serial.println(gateway);
  Serial.print("Subnet Mask: ");
  Serial.println(subnet);


  //tft.fillScreen(MYBLACK);
}
void scrollText(String text) {
  text += String(' ', tft.width());
  int textWidth = tft.textWidth(text);
  int textX = tft.width();                              // Start completely off-screen to the left
  //int textY = (SCREEN_HEIGHT - tft.fontHeight()) / 2;  // Center vertically
  int textY = 122;
  tft.setTextSize(3);
  tft.fillRect(0,120,tft.width(),30,TFT_BROWN);
  tft.setTextColor(TFT_GOLD,TFT_BROWN);
  tft.setTextWrap(false); // Don't wrap text to next line
  while (textX + textWidth > 0) {  // Move text across the screen
    tft.setCursor(textX, textY);
    tft.print(text);
    textX -= 5;  // Adjust scrolling speed
    delay(SCROLL_DELAY);
  


  }
}


void setup() {
  initializeQs();
  Serial.begin(115200);
  SD.begin(SD_CS);
  loadConfig();
  printConfigValues();

  //myTZ.setLocation(F("America/New_York"));
  Serial.println(myTZ.dateTime());
  setDebug(INFO);

 // FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
 // FastLED.setBrightness(bright);

  //delay(6000);
  tft.init();
  tft.setRotation(1);
  tft.invertDisplay(true);
  WiFi.begin(ssid.c_str(), password.c_str());
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Status...");
    Serial.println(WiFi.status());
    displayNetworkInfo();
  }
  waitForSync();


  Serial.println("Starting webserver");
  setupWebServer();
}

void loop() {
  String myTZName;
  if (wifiNeedsRestart) {
    Serial.println("Restarting...");
    delay(10000);
    WiFi.disconnect();
    WiFi.begin(ssid.c_str(), password.c_str());
    while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      printConfigValues();
      delay(600);
      Serial.println("Status...");
      Serial.println(WiFi.status());
      displayNetworkInfo();
      myTZName = myTZ.getTimezoneName();
      displayIP(myTZName);
    }
    Serial.println("Waiting for time sync.");
    waitForSync();
    wifiNeedsRestart = false;
  }
  if (tzUpdated) {
    myTZ.setLocation(timeZone);
    myTZName = myTZ.getTimezoneName();
    waitForSync();
    Serial.println("Updating Timezone to " + timeZone + "Shortname " + myTZName);
    displayIP(myTZName);
    tzUpdated = false;
  }


  if (lastMin != myTZ.dateTime("i")) {
    String formatTime = myTZ.dateTime("g:i A");
    displayTime(formatTime);
    lastMin = myTZ.dateTime("i");
  }

  if (newLEDs) {
  //  fill_solid(leds, NUM_LEDS, CRGB(red, green, blue));
  //  FastLED.setBrightness(bright);
  //  FastLED.show();
    newLEDs = false;
  }


  if (updatedCity) {
    fetchWeather();
    updatedCity = false;
  }


  if (now() > lastWck + wInterval) {
    fetchWeather();
    lastWck = now();
  }

  if (quoteOn){
    int randomIndex = random(0, qcount);
    Serial.println("Chose line number: " + String(randomIndex));
  for (int i = 0; i < REPEAT_COUNT; i++) {
    String text = Qlines[randomIndex];
    Serial.println("Text: "+ text);
    scrollText(text);
  }
  }

  delay(1000);  // Update every 1 secs
  events();
}
