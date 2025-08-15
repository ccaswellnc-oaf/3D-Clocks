//Program for getting date, time and weather for two locations and 
//presenting them on a "Cheap Yellow Display"
//Requires an API key from OpenWeatherMap.org
//The cobfiguration file is kept on a microSD card


//The accompanying 3D print files are meant to be printed with a clear filament
//Code for using LEDs within the clock for back lighting is mostly commented out because I couldn't get the power right

#include <WiFi.h>
#include <SPI.h>
#include <SD.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <FastLED.h>
#include <TFT_eSPI.h>
#include <HTTPClient.h>
#include <ezTime.h>

uint32_t MYGREEN = TFT_GREEN;
uint32_t MYBLUE = TFT_BLUE;
uint32_t MYRED = TFT_RED;
uint32_t MYYELLOW = TFT_YELLOW;

// Define hardware connections
#define LED_PIN 21  // Adjust as needed
#define NUM_LEDS 12  // Number of LEDs in the string
#define SD_CS 5     // SD Card Chip Select Pin

CRGB leds[NUM_LEDS];

// Initialize peripherals
TFT_eSPI tft = TFT_eSPI();
AsyncWebServer server(80);

Timezone myTZ;


int lastWck = 0;
int wInterval = 900000;  // update weather every 15 minutes

// Variables to store configuration
String ssid, password, apiKey;
String cityName, timeZone;
String city2Name, time2Zone;
String tScale, tScale2;
float latitude, longitude;
float latitude2, longitude2;
int red, green, blue, bright;
bool wifiNeedsRestart = false;
bool tzUpdated = true;
bool tsUpdated = false;
String lastMin = "AA";
bool updatedCity = true;
bool newLEDs = true;
String dateBoxstr;

String optcode = "<option value='Imperial' selected>Imperial</option> <option value='Metric' >Metric</option>";
String optcode2 = "<option value='Imperial' selected>Imperial</option> <option value='Metric' >Metric</option>";



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
    else if (line.startsWith("TEMP_SCALE=")) tScale = line.substring(11);
    else if (line.startsWith("CITY=")) cityName = line.substring(5);
    else if (line.startsWith("LAT2=")) latitude2 = line.substring(5).toFloat();
    else if (line.startsWith("LON2=")) longitude2 = line.substring(5).toFloat();
    else if (line.startsWith("TIMEZONE2=")) time2Zone = line.substring(10);
    else if (line.startsWith("CITY2=")) city2Name = line.substring(6);
    else if (line.startsWith("TEMP_SCALE2=")) tScale2 = line.substring(12);
    else if (line.startsWith("RED=")) red = constrain(line.substring(4).toInt(), 0, 255);
    else if (line.startsWith("GREEN=")) green = constrain(line.substring(6).toInt(), 0, 255);
    else if (line.startsWith("BLUE=")) blue = constrain(line.substring(5).toInt(), 0, 255);
    else if (line.startsWith("BRIGHTNESS=")) bright = constrain(line.substring(11).toInt(), 0, 255);
  }
  file.close();
}

void printConfigValues() {
  Serial.println("Configuration:");
  Serial.println("SSID:" + ssid);
  Serial.println("Password: " + password);
  Serial.println("APIKEY: " + apiKey);
  Serial.println("Latitude: " + String(latitude));
  Serial.println("Longitude: " + String(longitude));
  Serial.println("TZ: " + timeZone);
  Serial.println("City1: " + cityName);
  Serial.println("TScale1: " + tScale);
  Serial.println("Lat2: " + String(latitude2));
  Serial.println("Long2: " + String(longitude2));
  Serial.println("TZ2: " + time2Zone);
  Serial.println("City2: " + city2Name);
  Serial.println("Tscale2: " + tScale2);
  Serial.println("Red: " + String(red));
  Serial.println("Green: " + String(green));
  Serial.println("Blue: " + String(blue));
   Serial.println("Brightness: " + String(bright));
  //delay(100000);
}

// Save configuration to SD card
void saveConfig() {
  File file = SD.open("/setup.txt", FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open setup.txt for writing");
    return;
  }
  // Serial.printf("SSID=%s\nPASSWORD=%s\nAPIKEY=%s\nCITY=%s\nLAT=%.6f\nLON=%.6f\nTIMEZONE=%s\nTEMP_SCALE=%s\nCITY2=%s\nLAT2=%.6f\nLON2=%.6f\nTIMEZONE2=%s\nTEMP_SCALE2=%s\nRED=%d\nGREEN=%d\nBLUE=%d\nBRIGHTNESS=%d\n",
  //               ssid.c_str(), password.c_str(), apiKey.c_str(), cityName.c_str(), latitude, longitude, timeZone.c_str(), tScale.c_str(), city2Name.c_str(), latitude2, longitude2, time2Zone.c_str(), tScale2.c_str(), red, green, blue, bright);
  file.printf("SSID=%s\nPASSWORD=%s\nAPIKEY=%s\nCITY=%s\nLAT=%.6f\nLON=%.6f\nTIMEZONE=%s\nTEMP_SCALE=%s\nCITY2=%s\nLAT2=%.6f\nLON2=%.6f\nTIMEZONE2=%s\nTEMP_SCALE2=%s\nRED=%d\nGREEN=%d\nBLUE=%d\nBRIGHTNESS=%d\n",
              ssid.c_str(), password.c_str(), apiKey.c_str(), cityName.c_str(), latitude, longitude, timeZone.c_str(), tScale.c_str(), city2Name.c_str(), latitude2, longitude2, time2Zone.c_str(), tScale2.c_str(), red, green, blue,bright);
  file.close();
}



// Fetch weather data
void fetchWeather() {
  int16_t cWidth;
  int whichW, WyPos, CyPos;
  String url;
  String cityPrint;
  String tempFC;
  uint32_t color1, color2;

  if (WiFi.status() == WL_CONNECTED) {
    for (whichW = 0; whichW < 2; whichW++) {
      HTTPClient http;
      if (whichW > 0) {
        url = "http://api.openweathermap.org/data/2.5/weather?lat=" + String(latitude2) + "&lon=" + String(longitude2) + "&units=" + tScale2 + "&appid=" + apiKey;
        Serial.println("Weather URL1: " + url);
        WyPos = 200;
        CyPos = 180;
        cityPrint = city2Name;
        color1 = TFT_WHITE;
        color2 = TFT_BLUE;
        if (tScale2.equalsIgnoreCase("Metric")) {
          tempFC = "C";
        } else {
          tempFC = "F";
        }
      } else {
        url = "http://api.openweathermap.org/data/2.5/weather?lat=" + String(latitude) + "&lon=" + String(longitude) + "&units=" + tScale + "&appid=" + apiKey;
        Serial.println("Weather URL1: " + url);
        WyPos = 80;
        CyPos = 60;
        color1 = TFT_BLUE;
        color2 = TFT_WHITE;
        cityPrint = cityName;
        if (tScale.equalsIgnoreCase("Metric")) {
          tempFC = "C";
        } else {
          tempFC = "F";
        }
      }

      Serial.println(url);
      http.begin(url);
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
        // tft.fillScreen(TFT_WHITE);
        tft.setTextColor(color1, color2);
        //tft.setTextColor(TFT_BLACK, MYGREEN);
        tft.fillRect(0, CyPos, 320, 20, color2);
        //String cityString = "Weather for " + cityName;
        tft.setTextSize(2);

        cWidth = tft.textWidth(cityPrint);
        if (cWidth > tft.width()) {
          tft.setTextSize(1);
          cWidth = tft.textWidth(cityPrint);
        }
        int16_t csPos = (tft.width() - cWidth) / 2;
        tft.drawString(cityPrint, csPos, CyPos + 2);
        tft.setTextSize(2);
        tft.fillRect(0, WyPos, 320, 30, color2);
        String wsString = temperature + tempFC + " " + weather;
        int16_t wsWidth = tft.textWidth(wsString);
        int16_t wsPos = (tft.width() - wsWidth) / 2;
        tft.drawString(wsString, wsPos, WyPos + 2);
      }
      http.end();
    }
  }
}

// Put the clock on the screen
void displayTime(String clockStr, int whClock, String tZN) {
  int clYPos, dXPos, dYPos;
  uint32_t color1, color2;
  if (whClock > 0) {
    clYPos = 120;
    color1 = TFT_WHITE;
    color2 = MYBLUE;
  } else {
    clYPos = 0;
    color1 = MYBLUE;
    color2 = TFT_WHITE;
  }
  tft.fillRect(0, clYPos, 320, 60, color2);
  tft.setTextColor(color1, color2);
  tft.setTextSize(5);
  //Serial.println("tzName is " + myTZ.getTimezoneName());
  //String clockStr = myTZ.dateTime("g:i A");
  int16_t textWidth = tft.textWidth(clockStr);  // Get width of text
  int16_t x = (tft.width() - textWidth) / 2;    // Center X position
  tft.drawString(clockStr, x, clYPos + 15);
  int tzXPos = x + textWidth + 2;
  tft.setTextSize(1);
  tft.drawString(tZN, tzXPos, clYPos + 40);
}

void displayDate(int dWhich) {
  int dXPos, dYPos, bXPos;
  uint32_t color1, color2;

  tft.setTextSize(2);
  if (dWhich > 0) {
    dYPos = 105;
    bXPos = (tft.width() / 2);
    tft.setTextSize(2);
    dXPos = tft.width() - tft.textWidth(dateBoxstr);
    color1 = TFT_WHITE;
    color2 = MYBLUE;
  } else {
    dYPos = 105;
    dXPos = 0;
    bXPos = 0;
    color1 = MYBLUE;
    color2 = TFT_WHITE;
  }
  tft.fillRect(bXPos, dYPos, 160, 20, color2);
  tft.setTextColor(color1, color2);
  tft.drawString(dateBoxstr, dXPos, dYPos);
}


//Put the IP Address in the bottom corner
void displayIP() {
  IPAddress thisIP = WiFi.localIP();
  tft.fillRect(0, 220, 320, 20, MYBLUE);
  tft.setTextColor(TFT_WHITE, MYRED);
  tft.setTextSize(1);
  //tft.drawString(tzN, 2, 226);
  String ipStr = "IP:" + thisIP.toString();
  int16_t ipWidth = tft.textWidth(ipStr);  // Get width of text
  //Serial.println("Width of " + ipStr + "is " + ipWidth);
  int16_t x = (tft.width() - ipWidth) - 2;  // Right X position
  tft.drawString(ipStr, x, 232);
}

// Handle client requests
void setupWebServer() {


  if (tScale.equalsIgnoreCase("Metric")) {
    optcode = "<input type='radio' id='Imperial' name='tScale' value='Imperial'><label for='Imperial'>Imperial</label>";
    optcode += "<input type='radio' id='Metric' name='tScale' value='Metric' checked><label for='Metric'>Metric</label>";
  } else {
    optcode = "<input type='radio' id='Imperial' name='tScale' value='Imperial' checked><label for='Imperial'>Imperial</label>";
    optcode += "<input type='radio' id='Metric' name='tScale' value='Metric'><label for='Metric'>Metric</label>";
  }

  if (tScale2.equalsIgnoreCase("Metric")) {
    optcode2 = "<input type='radio' id='Imperial2' name='tScale2' value='Imperial'><label for='Imperial2'>Imperial</label>";
    optcode2 += "<input type='radio' id='Metric2' name='tScale2' value='Metric' checked><label for='Metric2'>Metric</label>";
  } else {
    optcode2 = "<input type='radio' id='Imperial2' name='tScale2' value='Imperial' checked><label for='Imperial2'>Imperial</label>";
    optcode2 += "<input type='radio' id='Metric2' name='tScale2' value='Metric'><label for='Metric2'>Metric</label>";
  }


  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    String html = "<html><body>"
                  "<h3>ESP32 Configuration</h3>"
                  "<form action='/update' method='POST'>"
                  "<h2>WiFi Setup</h2>"
                  "SSID: <input type='text' name='ssid' value='"
                  + ssid + "'><br>"
                           "Password: <input type='text' name='password'><br>"
                           "<h2>Weather API</h2>"
                           "See:<a 'https://www.openweathermap.org' target='_blank'>www.openweathermap.org</a><br>"
                           "API Key: <input type='text' name='apikey' value='"
                  + apiKey + "'><br>"
                             "<h2>Clock 1 </h2>"
                             "Location Name: <input type='text' name='city' value='"
                  + cityName + "'><br>"
                               "Latitude: <input type='text' name='latitude' value='"
                  + String(latitude) + "'><br>"
                                       "Longitude: <input type='text' name='longitude' value='"
                  + String(longitude) + "'><br>"
                                        "TimeZone Value must be in a format from <a href='https://en.wikipedia.org/wiki/List_of_tz_database_time_zones' target='_blank'>TZ Database Time Zones</a>"
                  + "<br>"
                    "Time Zone: <input type='text' name='timezone' value='"
                  + timeZone + "'><br>"
                               "Temp Units:"
                  + optcode
                  + "<h2>Clock 2 </h2>"
                    "Location Name: <input type='text' name='city2' value='"
                  + city2Name + "'><br>"
                                "Latitude: <input type='text' name='latitude2' value='"
                  + String(latitude2) + "'><br>"
                                        "Longitude: <input type='text' name='longitude2' value='"
                  + String(longitude2) + "'><br>"
                                         "TimeZone Value must be in a format from <a href='https://en.wikipedia.org/wiki/List_of_tz_database_time_zones' target='_blank'>TZ Database Time Zones</a>"
                  + "<br>"
                    "Time Zone: <input type='text' name='time2zone' value='"
                  + time2Zone + "'><br>"
                                "Temp Units:"
                  + optcode2
                  + "<h2>LED Settings:</h2><br>Color values should be between 0 and 255<br>"
                    "Red: <input type='number' name='red' min='0' max='255' value='"
                  + String(red) + "'><br>"
                                  "Green: <input type='number' name='green' min='0' max='255' value='"
                  + String(green) + "'><br>"
                                    "Blue: <input type='number' name='blue' min='0' max='255' value='"
                  + String(blue) + "'><br>Brightness should be between 0 and 100, with 0 = off<br>"
                                  "Brightness: <input type='bright' name='bright' min='0' max='100' value='"
                  + String(bright) + "'><br>"
                                   "<input type='submit' value='Update'>"
                                   "</form></body></html>";
    request->send(200, "text/html", html);
  });

  server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("ssid", true) || request->getParam("ssid", true)->value().isEmpty() || 
    !request->hasParam("password", true) || request->getParam("password", true)->value().isEmpty() || 
    !request->hasParam("apikey", true) || request->getParam("apikey", true)->value().isEmpty() || 
    !request->hasParam("latitude", true) || request->getParam("latitude", true)->value().isEmpty() || 
    !request->hasParam("longitude", true) || request->getParam("longitude", true)->value().isEmpty() || 
    !request->hasParam("timezone", true) || request->getParam("timezone", true)->value().isEmpty() || 
    !request->hasParam("city", true) || request->getParam("city", true)->value().isEmpty() || 
    !request->hasParam("latitude2", true) || request->getParam("latitude2", true)->value().isEmpty() || 
    !request->hasParam("longitude2", true) || request->getParam("longitude2", true)->value().isEmpty() || 
    !request->hasParam("time2zone", true) || request->getParam("time2zone", true)->value().isEmpty() || 
    !request->hasParam("city2", true) || request->getParam("city2", true)->value().isEmpty() || 
    !request->hasParam("red", true) || request->getParam("red", true)->value().isEmpty() || 
    !request->hasParam("green", true) || request->getParam("green", true)->value().isEmpty() || 
    !request->hasParam("blue", true) || request->getParam("blue", true)->value().isEmpty()
    ) 
    {
      String postErr = " < h1 > Error : All fields must be filled !</ h1><br> <p style='color:red'>Missing : ";
      if (!request->hasParam("ssid", true) || request->getParam("ssid", true)->value().isEmpty()) {
        postErr += "<br>SSID";
      }
      if (!request->hasParam("password", true) || request->getParam("password", true)->value().isEmpty()) {
        postErr += "<br>Password";
      }
      if (!request->hasParam("apikey", true) || request->getParam("apikey", true)->value().isEmpty()) {
        postErr += "<br>APIKEY";
      }
      if (!request->hasParam("city", true) || request->getParam("city", true)->value().isEmpty()) {
        postErr += "<br>Clock 1 Location";
      }
      if (!request->hasParam("latitude", true) || request->getParam("latitude", true)->value().isEmpty()) {
        postErr += "<br>Clock 1 LATITUDE";
      }
      if (!request->hasParam("longitude", true) || request->getParam("longitude", true)->value().isEmpty()) {
        postErr += "<br>Clock 1 LONGITUDE";
      }
      if (!request->hasParam("timezone", true) || request->getParam("timezone", true)->value().isEmpty()) {
        postErr += "<br>Clock 1 TimeZone";
      }
      if (!request->hasParam("city2", true) || request->getParam("city2", true)->value().isEmpty()) {
        postErr += "<br>Clock 2 Location";
      }
      if (!request->hasParam("latitude2", true) || request->getParam("latitude2", true)->value().isEmpty()) {
        postErr += "<br>Clock 2 LATITUDE";
      }
      if (!request->hasParam("longitude2", true) || request->getParam("longitude2", true)->value().isEmpty()) {
        postErr += "<br>Clock 2 LONGITUDE";
      }
      if (!request->hasParam("time2zone", true) || request->getParam("time2zone", true)->value().isEmpty()) {
        postErr += "<br>Clock 2 TimeZone";
      }
      if (!request->hasParam("red", true) || request->getParam("red", true)->value().isEmpty()) {
        postErr += "<br>Red";
      }
      if (!request->hasParam("green", true) || request->getParam("time2zone", true)->value().isEmpty()) {
        postErr += "<br>Clock 2 TimeZone";
      }
      if (!request->hasParam("time2zone", true) || request->getParam("time2zone", true)->value().isEmpty()) {
        postErr += "<br>Clock 2 TimeZone";
      
      }
      postErr += "<br></p><a href='/'>Back</a>";
      request->send(400, "text/html", postErr);
      return;
    }

    String newSSID = request->getParam("ssid", true)->value();
    String newPassword = request->getParam("password", true)->value();
    apiKey = request->getParam("apikey", true)->value();
    float newLatitude = request->getParam("latitude", true)->value().toFloat();
    float newLongitude = request->getParam("longitude", true)->value().toFloat();
    String newtimeZone = request->getParam("timezone", true)->value();
    String newcityName = request->getParam("city", true)->value();
    float newLatitude2 = request->getParam("latitude2", true)->value().toFloat();
    float newLongitude2 = request->getParam("longitude2", true)->value().toFloat();
    String newtime2Zone = request->getParam("time2zone", true)->value();
    String newcity2Name = request->getParam("city2", true)->value();
    String newTempU = request->getParam("tScale", true)->value();
    String newTempU2 = request->getParam("tScale2", true)->value();

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
    if (newLatitude != latitude || newLongitude != longitude || newLatitude2 != latitude2 || newLongitude2 != longitude2) {
      latitude = newLatitude;
      longitude = newLongitude;
      latitude2 = newLatitude2;
      longitude2 = newLongitude2;
      updatedCity = true;
    }
    if (newtimeZone != timeZone || newtime2Zone != time2Zone) {
      timeZone = newtimeZone;
      time2Zone = newtime2Zone;
      tzUpdated = true;
    }
    if (newcityName != cityName || newcity2Name != city2Name) {
      cityName = newcityName;
      city2Name = newcity2Name;
      updatedCity = true;
    }
    if (newTempU != tScale || newTempU2 != tScale2) {
      tScale = newTempU;
      tScale2 = newTempU2;
      tsUpdated = true;
    }
    if (newRed != red || newGreen != green || newBlue != blue || newBright != bright) {
      red = newRed;
      green = newGreen;
      blue = newBlue;
      bright = newBright;
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

  tft.fillScreen(MYGREEN);
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
  tft.fillScreen(MYRED);



  Serial.println("WiFi Connected:");
  Serial.print("IP Address: ");
  Serial.println(ip);
  Serial.print("Gateway: ");
  Serial.println(gateway);
  Serial.print("Subnet Mask: ");
  Serial.println(subnet);


  //tft.fillScreen(TFT_BLACK);
}

void setup() {
  Serial.begin(115200);
  SD.begin(SD_CS);
  loadConfig();

  //delay(5000);

  //myTZ.setLocation(F("America/New_York"));
  Serial.println(myTZ.dateTime());
  setDebug(INFO);
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
  printConfigValues();
 //FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  //FastLED.setBrightness(bright);



  setupWebServer();
}

void loop() {
  //  printConfigValues();
  String myTZName;
  if (wifiNeedsRestart) {
    WiFi.disconnect();
    WiFi.begin(ssid.c_str(), password.c_str());
    while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      printConfigValues();
      //delay(600);
      //Serial.println("Status...");
      //Serial.println(WiFi.status());
      //displayNetworkInfo();

      displayIP();
    }
    wifiNeedsRestart = false;
    waitForSync();
  }

  
  String formatTime;

  //if (lastMin != myTZ.dateTime("i")) {
  if (minuteChanged()) {
    myTZ.setLocation(timeZone);
    formatTime = myTZ.dateTime("g:i A");
    myTZName = myTZ.getTimezoneName();
    dateBoxstr = myTZ.dateTime("d-M-Y");
    displayTime(formatTime, 0, myTZName);
    displayDate(0);
    lastMin = myTZ.dateTime("i");
    //delay(3000);
    myTZ.setLocation(time2Zone);
    formatTime = myTZ.dateTime("g:i A");
    myTZName = myTZ.getTimezoneName();
    dateBoxstr = myTZ.dateTime("d-M-Y");
    displayTime(formatTime, 1, myTZName);
    displayDate(1);
    //delay(3000);
    myTZ.setLocation(timeZone);
  }

  if (tsUpdated) {
    fetchWeather();
    tsUpdated = false;
  }

  if (newLEDs) {
  //  fill_solid(leds, NUM_LEDS, CRGB(red, green, blue));
  //  FastLED.setBrightness(bright);
 //   FastLED.show();
    newLEDs = false;
  }


  if (updatedCity) {
    fetchWeather();
    updatedCity = false;
  }


  if (now() > lastWck + wInterval) {
    fetchWeather();
    displayIP();
    lastWck = now();
  }


  delay(1000);  // Update every 1 secs
  events();
}
