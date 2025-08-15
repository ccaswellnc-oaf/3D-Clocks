// CanesClockv20250316
// Caswell
// Built for ESP32 Dev board
// Arduino IDE v 2.3.4

//Program for getting date, time and weather for a location
//Program displays clock and next scheduled Carolina Hurricanes game
//until just before game time, then it displays a scoreboard
//By adjusting colors and the team abbreviation in URLS, it could easily be adapted for other teams
//The screen flashes when Carolina scores, and if they win
//Some code is stubbed in for LEDS within the clock body, but this is
//not successfully implemented
//Requires an API key from OpenWeatherMap.org
//The cobfiguration file is kept on a microSD card


#include <WiFi.h>  //WiFiEsp v 2.2.2
#include <SPI.h>   // TFT_eSPI 2.5.43 Bodmer
#include <SD.h>  // SD by Arduino SparkFun v 1.3.0
#include <ArduinoJson.h> // by Blanchon v 7.3.1
#include <ESPAsyncWebServer.h> // by ESP32 Async v 3.7.0
//#include <FastLED.h>
#include <TFT_eSPI.h>  // TFT_eSPI 2.5.43 Bodmer
#include <HTTPClient.h> v.2.2/0 Adrian McEwen
#include <ezTime.h> // v 0.8.3

#define BUFFPIXEL 20
uint32_t MYGREEN = 0x58b430;
uint32_t MYBLUE = TFT_BLUE;
uint32_t MYRED = TFT_MAROON;
uint32_t MYYELLOW = 0xffd966;
uint32_t MYBLACK = TFT_BLACK;
uint32_t MYWHITE = TFT_WHITE;

// Define hardware connections
#define LED_PIN 21  // Adjust as needed
#define NUM_LEDS 10  // Number of LEDs in the string
#define SD_CS 5     // SD Card Chip Select Pin
//#define BASEBRIGHT 25
//#define MAXBRIGHT 100


// Initialize peripherals

TFT_eSPI tft = TFT_eSPI();

//CRGB leds[NUM_LEDS];
Timezone myTZ;



// Variables to store configuration
String ssid, password, apiKey, cityName;
float latitude, longitude;
int lastWck = 0;
int wInterval = 900000;  // update weather every 15 minutes
const char* baseUrl = "https://api-web.nhle.com/v1/club-schedule/CAR/week/";
const int maxWeeksAhead = 4;  // Check up to 4 weeks ahead if no game is found
bool gameFound = false;
bool schedErr = false;
bool newSched = true;
int lastSch = 0;
int schInterval = 28800000;  // update schedule every 8 hours
//String lastMin = "AA";
String gameTime;
time_t gmCkTime;
time_t progStartTime;
String myTZName;
bool reStart = true;
bool theyWon = false;
bool theyScored = false;
int gameOn = 0;
bool updatePer = false;
bool updateScore = false;
bool isRed = true;
String formatTime;
bool gameFinal = false;

// Variables to store game data
String gameStatus, gameStatusOld, periodType, periodTypeOld;
int currentPeriod, currentPeriodOld;
String timeRemaining, timeRemainingOld, homeName, awayName;
int homeScore, homeScoreOld;
int awayScore, awayScoreOld;
int homeShots, homeShotsOld;
int awayShots, awayShotsOld;
int gameDayNo;
String clockINT, clockINTOld;
// NHL API host
const char* host = "api-web.nhle.com";

struct GameInfo {
  const char* gameDateUTC;
  String homeTeam;
  String awayTeam;
  String gameId;
  bool found;
} nextGame;

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
    else if (line.startsWith("CITY=")) cityName = line.substring(5);
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
  Serial.println(cityName);
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
  delay(5000);
  tft.fillScreen(MYRED);



  Serial.println("WiFi Connected:");
  Serial.print("IP Address: ");
  Serial.println(ip);
  Serial.print("Gateway: ");
  Serial.println(gateway);
  Serial.print("Subnet Mask: ");
  Serial.println(subnet);


  //tft.fillScreen(MYBLACK);
}

bool findNextGame() {
  String ckDate = myTZ.dateTime("Y-m-d");
  time_t urlDate = myTZ.now();
  nextGame.found = false;
  for (int i = 0; i <= 31; i++) {
    //String url = baseUrl + getFutureDate(i * 7);  // Get the schedule for each week ahead

    String url = baseUrl + ckDate;  // Get the schedule for each week ahead
                                    // Serial.println("Checking: " + url);

    if (fetchAndParseSchedule(url)) {
      //   Serial.println("Upcoming game found!");
      return true;
    }
    urlDate += 86400;
    ckDate = dateTime(urlDate, "Y-m-d");
  }
  Serial.println("No games found in the next month.");
  schedErr = true;
  displayErr("No upcoming games found.");
}

// Fetch data and parse JSON
bool fetchAndParseSchedule(String url) {
  HTTPClient http;
  Serial.println("Checking schedule using: " + url);
  http.begin(url);
  //Serial.println(url);
  int httpCode = http.GET();

  if (httpCode > 0) {
    String payload = http.getString();
    http.end();
    return parseJson(payload);
  } else {
    Serial.println(url);
    Serial.println("HTTP request failed: " + String(httpCode));
    http.end();
    schedErr = true;
    String errorSt = "HTTP request failed: " + String(httpCode);
    displayErr(errorSt);
    return false;
  }
}

// Parse JSON response
bool parseJson(String jsonString) {
  StaticJsonDocument<4096> doc;
  DeserializationError error = deserializeJson(doc, jsonString);
  if (error) {
    Serial.println("JSON parsing failed!");
    displayErr("JSON error parsing schedule.");
    return false;
  }

  JsonArray games = doc["games"].as<JsonArray>();
  if (games.size() == 0) {
    Serial.println("No games this week.");
    return false;
  }

  for (JsonObject game : games) {
    nextGame.gameDateUTC = game["startTimeUTC"];
    Serial.print(myTZ.dateTime("H:m:s:v") + ": Found next game at: ");
    Serial.println(nextGame.gameDateUTC);
    // nextGame.time = game["gameTimeUTC"].as<String>();
    // nextGame.gameDateUTC = game["gameDate"]; // Example: "2024-02-28T00:00:00Z"
    nextGame.homeTeam = game["homeTeam"]["abbrev"].as<String>();
    nextGame.awayTeam = game["awayTeam"]["abbrev"].as<String>();
    nextGame.gameId = game["id"].as<String>();
    nextGame.found = true;

    //time_t utcGTime = parseISO8601(nextGame.gameDateUTC);
    //Serial.println("utcGTime is " + dateTime(utcGTime,"M-d-y   H:i"));
    //Serial.println(utcGTime);

    gameTime = convertUTCToLocal(nextGame.gameDateUTC);

    //Serial.println("gameTime is " + dateTime(gameTime,"M-d-y   H:i"));
    //Serial.println(gameTime);
    // delay(5000);

    Serial.println("Next Game Details: " + myTZ.dateTime("[H:m:s:v]"));
    Serial.print("Date: ");
    Serial.println(gameTime);
    Serial.println("Home Team: " + nextGame.homeTeam);
    Serial.println("Away Team: " + nextGame.awayTeam);
    Serial.println("Game ID: " + nextGame.gameId);
    return true;
  }
  return false;
}

String convertUTCToLocal(const char* utcDateTime) {


  // Parse the UTC date-time string (ISO 8601 format)
  int year, month, day, hour, minute, second;
  sscanf(utcDateTime, "%4d-%2d-%2dT%2d:%2d:%2dZ", &year, &month, &day, &hour, &minute, &second);

  Serial.println("Parsed time from string:");
  Serial.println("Year: " + String(year));
  Serial.println("Month: " + String(month));
  Serial.println("Day: " + String(day));
  Serial.println("Hour: " + String(hour));
  Serial.println("Minute: " + String(minute));
  Serial.println("Second: " + String(second));



  // Convert parsed values into a UNIX timestamp (seconds since 1970)
  //time_t utcTimestamp = makeTime(second, minute, hour, day, month, year);
  time_t utcTimestamp = makeTime(hour, minute, second, day, month, year);  


  Serial.println("Converted utcTimestamp is: " + myTZ.dateTime(utcTimestamp, "m-d-Y h:i a"));
  int myOffset = myTZ.getOffset();

  int16_t offTime = (myOffset * 60);
  //int16_t offTime = (myOffset);

  utcTimestamp -= offTime;
  //Serial.println("Offset is: " + String(myOffset));
  
  //Serial.println("New utcTimestamp: " + myTZ.dateTime(utcTimestamp, "m-d-Y H:i"));
  gmCkTime = utcTimestamp - 900;

  Serial.println("gmCkTime: " + myTZ.dateTime(gmCkTime, "m-d-Y H:i"));

   //delay(60000);

  // Convert UTC timestamp to local time using EZTime
  // time_t localTimestamp = utcTimestamp + myTZ.timeZone(utcTimestamp);
  // Format and return the local time
  return myTZ.dateTime(utcTimestamp, "M-d-Y h:ia");
}


void DisplaySched() {
  String gmStr1, gmStr2;
  int16_t ipWidth, x;

  Serial.println("Displaying schedule: " + myTZ.dateTime("H:m:s:v"));
  tft.fillRect(0, 157, 320, 100, MYRED);
  tft.setTextColor(MYWHITE, MYRED);
  tft.setTextSize(2);
  if (!gameFound) {
   // gmStr1 = "  ";
   // gmStr2 = "No upcoming games.";
   bmpDraw("/Car_small.bmp", 115, 175);
  } else {
    //   gmStr1 =  nextGame.gameDateUTC;
    gmStr1 = gameTime;
    gmStr2 = nextGame.awayTeam + " at " + nextGame.homeTeam;
  }

  ipWidth = tft.textWidth(gmStr1);  // Get width of text
  x = (tft.width() - ipWidth) / 2;  // Right X position
  tft.drawString(gmStr1, x, 164);
  tft.setTextSize(4);
  ipWidth = tft.textWidth(gmStr2);  // Get width of text
  x = (tft.width() - ipWidth) / 2;  // Right X position
  tft.drawString(gmStr2, x, 184);
  tft.setTextSize(2);
}

void DisplayBoxScore() {
  String periodStr, sogStr;
  int16_t ipWidth, x, boxOff;
  int boxWidth = 80;
  int boxHgt = 120;

  // Serial.println("Displaying box score at: " + myTZ.dateTime("H:m:s:v"));
  Serial.println(myTZ.dateTime("H:m:s:v") + " GameOn=" + String(gameOn));
  // delay(5000);

  if (updatePer || gameOn == 2) {
    tft.fillRect(0, 55, tft.width(), 26, MYBLACK);
    tft.setTextColor(MYWHITE, MYBLACK);
    // Game Status

    if (!gameStatus.equals("FUT") && !gameStatus.equals("FINAL")) {
      if (gameStatus.equals("CRIT")){
        gameStatus = "LIVE";
      }
      tft.setTextSize(1);
      
      tft.drawString("P:", 2, 64);
      tft.setTextSize(3);
      if (!periodType.equals("REG")) {
        periodStr = periodType;
      } else {
        periodStr = String(currentPeriod);
      }

      Serial.println("Updating period info: " + myTZ.dateTime("H:m:s:v"));
      tft.drawString(periodStr,14, 57);

      tft.setTextSize(3);
      if (clockINT.equals("true")) {
        periodStr = " INT";
      } else {
        periodStr = " " + timeRemaining;
      }
      ipWidth = tft.textWidth(periodStr);  // Get width of text
      x = ((tft.width() - ipWidth) / 2);
      Serial.println("Updating time remaining: " + myTZ.dateTime("H:m:s:v"));
      tft.drawString(periodStr, x, 57);


      tft.setTextSize(3);
      if (!gameStatus.equals("OFF")) {
        ipWidth = tft.textWidth(gameStatus);
        x = ((tft.width() - ipWidth) - 2);
        Serial.println("Updating game status: " + myTZ.dateTime("H:m:s:v"));
        tft.drawString(gameStatus, x, 57);
      }

    } else {
      tft.setTextSize(3);
      if (gameStatus == "FUT") {
        periodStr = "Starting Soon:";
      } else {
        periodStr = gameStatus;
      }
      ipWidth = tft.textWidth(periodStr);  // Get width of text
      x = ((tft.width() - ipWidth) / 2);
      Serial.println("Updating time remaining: " + myTZ.dateTime("H:m:s:v"));
      tft.drawString(periodStr, x, 57);
    }
  }
  if (updateScore || gameOn == 2) {
    tft.fillRect(0, 81, tft.width(), 160, MYRED);
    // Home and Away boxes
    boxOff = 70;
    tft.fillRect(boxOff, 90, boxWidth, boxHgt, MYWHITE);
    if (awayName.equals("CAR")) {
      tft.setTextColor(MYRED, MYBLACK);
    } else {
      tft.setTextColor(MYBLUE, MYBLACK);
    }
    tft.setTextSize(3);
    x = ((boxWidth - tft.textWidth(awayName)) / 2) + boxOff;
    Serial.println("Updating score boxes: " + myTZ.dateTime("H:m:s:v"));
    tft.drawString(awayName, x, 92);
    tft.setTextSize(8);
    if (awayName.equals("CAR")) {
      tft.setTextColor(MYRED, MYWHITE);
    } else {
      tft.setTextColor(MYBLUE, MYWHITE);
    }
    x = ((boxWidth - tft.textWidth(String(awayScore))) / 2) + boxOff;
    tft.drawString(String(awayScore), x, 125);
    tft.setTextSize(2);
    sogStr = "SOG:" + String(awayShots);
    x = ((boxWidth - tft.textWidth(String(sogStr))) / 2) + boxOff;
    tft.drawString(String(sogStr), x, 190);
    tft.setTextColor(MYWHITE, MYBLACK);
    tft.drawString("AWAY", x+5, 214);


    boxOff = 190;
    tft.fillRect(boxOff, 90, boxWidth, boxHgt, MYWHITE);
    if (homeName.equals("CAR")) {
      tft.setTextColor(MYRED, MYWHITE);
    } else {
      tft.setTextColor(MYBLUE, MYWHITE);
    }
    tft.setTextSize(3);
    x = ((boxWidth - tft.textWidth(homeName)) / 2) + boxOff;
    tft.drawString(homeName, x, 92);
    tft.setTextSize(7);
    if (homeName.equals("CAR")) {
      tft.setTextColor(MYRED, MYWHITE);
    } else {
      tft.setTextColor(MYBLUE, MYWHITE);
    }
    x = ((boxWidth - tft.textWidth(String(homeScore))) / 2) + boxOff;
    tft.drawString(String(homeScore), x, 125);
    tft.setTextSize(2);
    sogStr = "SOG:" + String(homeShots);
    x = ((boxWidth - tft.textWidth(String(sogStr))) / 2) + boxOff;
    tft.drawString(String(sogStr), x, 190);
    tft.setTextColor(MYWHITE, MYBLACK);
    tft.drawString("HOME", x+5, 214);
  }
  updatePer = false;
  updateScore = false;
}

void fetchGameStats(String gameId) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient client;

if (!gameFinal){
    String url = "https://api-web.nhle.com/v1/gamecenter/" + gameId + "/boxscore";
    Serial.println(myTZ.dateTime("H:m:s:v") + ": Requesting: " + url);
    client.begin(url);
    int clientCode = client.GET();
    //Serial.print("Client code is: ");
    //Serial.println(clientCode);
    if (clientCode > 0) {

      String response = client.getString();
      //Serial.println(response);
      DynamicJsonDocument doc(20240);
      deserializeJson(doc, response);
      client.end();

      // Check for changes
      gameStatusOld = gameStatus;
      currentPeriodOld = currentPeriod;
      periodTypeOld = periodType;
      timeRemainingOld = timeRemaining;
      clockINTOld = clockINT;
      homeScoreOld = homeScore;
      awayScoreOld = awayScore;
      homeShotsOld = homeShots;
      awayShotsOld = awayShots;

      // Extract values
      if (gameStatus){
      gameStatus = doc["gameState"].as<String>();
      }else {gameStatus = " ";}
      
      if (currentPeriod){
      currentPeriod = doc["periodDescriptor"]["number"].as<int>();
      } else {currentPeriod =0;}
      
      if (periodType){
      periodType = doc["periodDescriptor"]["periodType"].as<String>();
      } else {periodType = " ";}
      
      if (timeRemaining){
      timeRemaining = doc["clock"]["timeRemaining"].as<String>();
      } else {timeRemaining = " ";}
      
      if (clockINT) {
      clockINT = doc["clock"]["inIntermission"].as<String>();
      } else {clockINT = " ";}
      
      if (homeName){
      homeName = doc["homeTeam"]["abbrev"].as<String>();
      } else {homeName = " ";}
      
      if (awayName){
      awayName = doc["awayTeam"]["abbrev"].as<String>();
      
      if (homeScore){
      homeScore = doc["homeTeam"]["score"].as<int>();
      } else {homeScore = 0;}

      if (awayScore){
      awayScore = doc["awayTeam"]["score"].as<int>();
      } else {awayScore = 0;}
      
      if (homeShots){
      homeShots = doc["homeTeam"]["sog"].as<int>();
      } else {homeShots = 0;}
      
      if (awayShots){}
      awayShots = doc["awayTeam"]["sog"].as<int>();
      } else {awayShots = 0;}

   
      if (gameStatus != gameStatusOld || currentPeriod != currentPeriodOld || periodType != periodTypeOld || clockINT != clockINTOld || timeRemaining != timeRemainingOld) {
        updatePer = true;
        Serial.println("gameStatus " + gameStatusOld + "|" + gameStatus);
        Serial.println("currentPeriod" + String(currentPeriodOld) + "|" + String(currentPeriod));
        Serial.println("timeRemaining" + timeRemainingOld + "|" + timeRemaining);
      }
      if (homeScore != homeScoreOld || awayScore != awayScoreOld || homeShots != homeShotsOld || awayShots != awayShotsOld) {
        updateScore = true;
      }
      if (homeName.equals("CAR") && homeScore > homeScoreOld && gameOn > 2) {
        Serial.println("Triggering score - Game on is : " + String(gameOn));
        theyScored = true;
      }
      if (awayName.equals("CAR") && awayScore > awayScoreOld && gameOn > 2) {
        theyScored = true;
        Serial.println("Triggering score - Game on is : " + String(gameOn));
      }
      if (gameStatus != gameStatusOld && gameStatus.equals("FINAL")) {
        if (homeName.equals("CAR") && homeScore > awayScoreOld && gameOn > 2) {
          theyWon = true;
        }
        if (awayName.equals("CAR") && awayScore > homeScoreOld && gameOn > 2) {
          theyWon = true;
        }
      }

      // Print stats
      Serial.println(myTZ.dateTime("H:m:s:v") + "Game Status: " + gameStatus);
      Serial.println("Current Period: " + String(currentPeriod));
      Serial.println("Time Remaining: " + timeRemaining);
      Serial.println("Home Score: " + String(homeScore));
      Serial.println("Away Score: " + String(awayScore));
      Serial.println("Home Shots: " + String(homeShots));
      Serial.println("Away Shots: " + String(awayShots));
    } else
      displayErr("Couldn't get game stats");
  }
  
  Serial.print(myTZ.dateTime("H:m:s:v") + "POST updatePer: ");
  Serial.println(updatePer);
  Serial.print(myTZ.dateTime("H:m:s:v") + "POST updateScore: ");
  Serial.println(updateScore);
  }
   if (gameStatus.equals("FINAL") || gameStatus.equals("OFF")){
        gameFinal = true;
      }

}


void canesWin(String goalWin) {
  int textWidth;
  int flDelay = 50;
  int x;


  for (int fl = 0; fl <= 50; fl++) {
    tft.fillRect(0, 0, tft.width(), 240, MYWHITE);
    tft.setTextColor(MYRED, MYWHITE);
    tft.setTextSize(8);
    textWidth = tft.textWidth("CANES");
    x = (tft.width() - textWidth) / 2;  // Center X position
    tft.drawString("CANES", x, 40);
    textWidth = tft.textWidth(goalWin);
    x = (tft.width() - textWidth) / 2;  // Center X position
    tft.drawString(goalWin, x, 100);
    delay(flDelay);
    tft.fillRect(0, 0, tft.width(), 240, MYBLACK);
    tft.setTextColor(MYRED, MYBLACK);
    textWidth = tft.textWidth("CANES");
    x = (tft.width() - textWidth) / 2;  // Center X position
    tft.drawString("CANES", x, 40);
    textWidth = tft.textWidth(goalWin);
    x = (tft.width() - textWidth) / 2;  // Center X position
    tft.drawString(goalWin, x, 100);
    delay(flDelay);
    tft.fillRect(0, 0, tft.width(), 240, MYBLACK);
    tft.setTextColor(MYWHITE, MYBLACK);
    textWidth = tft.textWidth("CANES");
    x = (tft.width() - textWidth) / 2;  // Center X position
    tft.drawString("CANES", x, 40);
    textWidth = tft.textWidth(goalWin);
    x = (tft.width() - textWidth) / 2;  // Center X position
    tft.drawString(goalWin, x, 100);
    delay(flDelay);
    tft.fillRect(0, 0, tft.width(), 240, MYRED);
    tft.setTextColor(MYWHITE, MYRED);
    textWidth = tft.textWidth("CANES");
    x = (tft.width() - textWidth) / 2;  // Center X position
    tft.drawString("CANES", x, 40);
    textWidth = tft.textWidth(goalWin);
    x = (tft.width() - textWidth) / 2;  // Center X position
    tft.drawString(goalWin, x, 100);
    delay(flDelay);
    tft.fillScreen(MYRED);

    theyWon = false;
  }
}
// Put the clock on the screen
void displayTime(String clockStr) {
  int16_t textWidth, x;

  if (gameOn < 1) {
    String dtStr = myTZ.dateTime("D, M jS, Y (T)");
    tft.fillRect(0, 0, tft.width(), 90, MYRED);
    tft.setTextColor(MYWHITE, MYRED);
    tft.setTextSize(2);
    textWidth = tft.textWidth(dtStr);
    x = (tft.width() - textWidth) / 2;  // Center X position
    tft.drawString(dtStr, x, 10);
    tft.setTextSize(6);
    textWidth = tft.textWidth(clockStr);  // Get width of text
    x = (tft.width() - textWidth) / 2;    // Center X position
    tft.drawString(clockStr, x, 48);
    tft.setTextSize(2);
  } else {
    String dtStr = myTZ.dateTime("m-d-Y h:i a (T)");
    tft.fillRect(0, 0, tft.width(), 30, MYRED);
    tft.setTextColor(MYWHITE, MYRED);
    tft.setTextSize(2);
    textWidth = tft.textWidth(dtStr);
    x = (tft.width() - textWidth) / 2;  // Center X position
    tft.drawString(dtStr, x, 10);
  }
}

// Fetch weather data
void fetchWeather() {
  String wsString;
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "http://api.openweathermap.org/data/2.5/weather?lat=" + String(latitude) + "&lon=" + String(longitude) + "&units=imperial&appid=" + apiKey;
    Serial.println(myTZ.dateTime("H:m:s:v") + ": Checking weather: " + url);
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
      // tft.fillScreen(MYWHITE);
      if (gameOn < 1) {
        tft.setTextColor(MYWHITE, MYBLACK);
        tft.fillRect(0, 105, tft.width(), 42, MYBLACK);
        tft.setTextSize(2);
        int16_t cWidth = tft.textWidth(cityName);
        if (cWidth > tft.width()) {
          tft.setTextSize(1);
          cWidth = tft.textWidth(cityName);
        }
        int16_t csPos = (tft.width() - cWidth) / 2;
        tft.drawString(cityName, csPos, 110);
        tft.setTextSize(2);
        // tft.fillRect(0, 180, 320, 20, MYBLUE);
        if (temperature == "null" || weather == "null") {
          tft.setTextSize(1);
          tft.setTextColor(MYRED, MYBLACK);
          wsString = "Error retrieving weather data";
        } else {
          wsString = temperature + "F " + weather;
        }
        // String wsString = temperature + "F " + weather;
        int16_t wsWidth = tft.textWidth(wsString);
        int16_t wsPos = (tft.width() - wsWidth) / 2;
        tft.drawString(wsString, wsPos, 130);
      } else {
        tft.setTextSize(2);
        tft.fillRect(0, 30, tft.width(), 25, MYRED);
        if (temperature == "null" || weather == "null") {
          tft.setTextSize(1);
          tft.setTextColor(MYWHITE, MYBLACK);
          wsString = "Error retrieving weather data";
        } else {
          wsString = cityName + " " + temperature + "F " + weather;
        }
        // String wsString = temperature + "F " + weather;
        int16_t wsWidth = tft.textWidth(wsString);
        int16_t wsPos = (tft.width() - wsWidth) / 2;
        tft.drawString(wsString, wsPos, 32);
      }
      http.end();
    }
  }
}

void displayErr(String errStr) {
  tft.fillRect(0, 142, 320, 100, MYRED);
  tft.setTextColor(MYBLACK, MYRED);
  tft.setTextSize(2);
  tft.drawString(errStr, 2, 110);
}




void setup() {
  Serial.begin(115200);
  SD.begin(SD_CS);
  loadConfig();
  printConfigValues();
  //delay(5000);
  

  // myTZ.setLocation(F("America/New_York"));
  //  Serial.println(myTZ.dateTime());
  setDebug(INFO);
  //delay(6000);
  tft.init();
  tft.setRotation(1);
  tft.invertDisplay(true);
  WiFi.begin(ssid.c_str(), password.c_str());
  while (WiFi.status() != WL_CONNECTED) {
    // delay(1000);
    Serial.println("Status...");
    Serial.println(WiFi.status());
   
  }
   displayNetworkInfo();
   //delay(3000);
   bmpDraw("/Car_dark.bmp", 75, 70);
   delay(3000);
   tft.fillScreen(MYRED);
  //gameFound = findNextGame();
  waitForSync();
  myTZ.setLocation(F("America/New_York"));
  myTZ.setDefault();
  progStartTime = now();
 // FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  //FastLED.setBrightness(BASEBRIGHT);
  //fill_solid(leds, NUM_LEDS, CRGB::White);

  // gameFound = findNextGame();
}

void loop() {
  if (reStart) {
    waitForSync();
    myTZ.setLocation(F("America/New_York"));
    myTZ.setDefault();

    String formatTime = myTZ.dateTime("g:i A");
    displayTime(formatTime);


    myTZName = myTZ.getTimezoneName();
    // gameFound = findNextGame();
    //fetchWeather();
    reStart = false;
  }

Serial.println("Start of Loop GameOn = " + String(gameOn));
//delay(10000);

  if (minuteChanged()) {
    String formatTime = myTZ.dateTime("g:i A");
    Serial.println("Updating clock at: " + myTZ.dateTime("H:m:s:v"));
    displayTime(formatTime);
    //lastMin = myTZ.dateTime("i");
  }


  if (now() > lastWck + wInterval) {
    fetchWeather();
    lastWck = now();  
  }

  if (gameOn < 1) {
    if (now() > lastSch + schInterval) {
      schedErr = false;
      gameFound = findNextGame();
      lastSch = now();
      DisplaySched();
    }


    if (now() > gmCkTime) {  // switch to game mode 15 minutes before game
      Serial.println("gmCkTime " + myTZ.dateTime(gmCkTime, "d-m-Y g:i A"));
      Serial.println("GAME ON! at " + myTZ.dateTime("g:i:s A"));
       gameOn++;
     //delay(50000);
      tft.fillScreen(MYRED);
    String formatTime = myTZ.dateTime("g:i A");
    Serial.println("Updating clock at: " + myTZ.dateTime("H:m:s:v"));
    displayTime(formatTime);

    
    }
  }

  if (gameOn > 0) {
    fetchGameStats(nextGame.gameId);
    gameOn++;
    Serial.print("GameOn after checking stats =" + String(gameOn));
    Serial.print("Checking stats at: ");
    Serial.println(myTZ.dateTime("H:m:s:v"));
    if (gameOn < 2){fetchWeather();}
    DisplayBoxScore();



    if (theyScored) {
      //FastLED.setBrightness(MAXBRIGHT);
 

      if ((now() - progStartTime) > 30) {
        canesWin("GOAL!!");
      }
      theyScored = false;
      //FastLED.setBrightness(BASEBRIGHT);
      //fill_solid(leds, NUM_LEDS, CRGB::White);
      tft.fillScreen(MYRED);
      formatTime = myTZ.dateTime("g:i A");
      Serial.println("Updating clock at: " + myTZ.dateTime("H:m:s:v"));
      displayTime(formatTime);
      fetchWeather();
      gameOn++;
      fetchGameStats(nextGame.gameId);
    }

    if (theyWon) {
      if ((now() - progStartTime) > 30) {
           
        canesWin("WIN!!");
      }
      tft.fillScreen(MYRED);
      formatTime = myTZ.dateTime("g:i A");
      Serial.println("Updating clock at: " + myTZ.dateTime("H:m:s:v"));
      displayTime(formatTime);
      fetchWeather();
      gameOn++;
      fetchGameStats(nextGame.gameId);
      theyWon = false;
      //FastLED.setBrightness(BASEBRIGHT);
      //fill_solid(leds, NUM_LEDS, CRGB::White);

      formatTime = myTZ.dateTime("g:i A");
      Serial.println("Updating clock at: " + myTZ.dateTime("H:m:s:v"));
      displayTime(formatTime);
      fetchWeather();
      gameOn++;
      fetchGameStats(nextGame.gameId);
    }

    if (gameStatus == "OFF") {
      gameOn = 0;
      gameFinal = false;
    }
  }



  delay(10000);  // Update every 10 secs
  events();
}

void bmpDraw(String filename, uint8_t x, uint16_t y) {

  File bmpFile;
  int bmpWidth, bmpHeight;             // W+H in pixels
  uint8_t bmpDepth;                    // Bit depth (currently must be 24)
  uint32_t bmpImageoffset;             // Start of image data in file
  uint32_t rowSize;                    // Not always = bmpWidth; may have padding
  uint8_t sdbuffer[3 * BUFFPIXEL];     // pixel buffer (R+G+B per pixel)
  uint8_t buffidx = sizeof(sdbuffer);  // Current position in sdbuffer
  boolean goodBmp = false;             // Set to true on valid header parse
  boolean flip = true;                 // BMP is stored bottom-to-top
  int w, h, row, col;
  uint8_t r, g, b;
  uint32_t pos = 0, startTime = millis();

  if ((x >= tft.width()) || (y >= tft.height())) return;

  Serial.println();
  Serial.print(F("Loading image '"));
  Serial.print(filename);
  Serial.println('\'');

  // Open requested file on SD card
  if ((bmpFile = SD.open(filename)) == NULL) {
    Serial.print(F("File not found"));
    return;
  }

  // Parse BMP header
  if (read16(bmpFile) == 0x4D42) {  // BMP signature
                                    //  Serial.print(F("File size: "));
    Serial.println(read32(bmpFile));
    (void)read32(bmpFile);             // Read & ignore creator bytes
    bmpImageoffset = read32(bmpFile);  // Start of image data
                                       // Serial.print(F("Image Offset: "));
                                       // Serial.println(bmpImageoffset, DEC);
    // Read DIB header
    // Serial.print(F("Header size: "));
    Serial.println(read32(bmpFile));
    bmpWidth = read32(bmpFile);
    bmpHeight = read32(bmpFile);
    Serial.println("Bitmap Width ");
    Serial.println(bmpWidth);
    Serial.println("Bitmap Height ");
    Serial.println(bmpHeight);
    
    if (read16(bmpFile) == 1) {                          // # planes -- must be '1'
      bmpDepth = read16(bmpFile);                        // bits per pixel
                                                         //   Serial.print(F("Bit Depth: "));
                                                         //   Serial.println(bmpDepth);
      if ((bmpDepth == 24) && (read32(bmpFile) == 0)) {  // 0 = uncompressed

        goodBmp = true;  // Supported BMP format -- proceed!
                         //     Serial.print(F("Image size: "));
                         //     Serial.print(bmpWidth);
                         //     Serial.print('x');
                         //     Serial.println(bmpHeight);

        // BMP rows are padded (if needed) to 4-byte boundary
        rowSize = (bmpWidth * 3 + 3) & ~3;
        //       Serial.print(F("row size: "));
        //       Serial.print(rowSize);

        // If bmpHeight is negative, image is in top-down order.
        // This is not canon but has been observed in the wild.
        if (bmpHeight < 0) {
          bmpHeight = -bmpHeight;
          flip = false;
          //         Serial.print(F("Flip Status: "));
          //        Serial.print(flip);
        }

        // Crop area to be loaded
        w = bmpWidth;
        h = bmpHeight;
        if ((x + w - 1) >= tft.width()) w = tft.width() - x;
        if ((y + h - 1) >= tft.height()) h = tft.height() - y;


        // Set TFT address window to clipped image bounds
        tft.startWrite();  // Start TFT transaction
        tft.setAddrWindow(x, y, w, h);

        for (row = 0; row < h; row++) {  // For each scanline...

          // Seek to start of scan line.  It might seem labor-
          // intensive to be doing this on every line, but this
          // method covers a lot of gritty details like cropping
          // and scanline padding.  Also, the seek only takes
          // place if the file position actually needs to change
          // (avoids a lot of cluster math in SD library).
          if (flip)  // Bitmap is stored bottom-to-top order (normal BMP)
            pos = bmpImageoffset + (bmpHeight - 1 - row) * rowSize;
          else  // Bitmap is stored top-to-bottom
            pos = bmpImageoffset + row * rowSize;


          if (bmpFile.position() != pos) {  // Need seek?
            tft.endWrite();                 // End TFT transaction
            bmpFile.seek(pos);
            buffidx = sizeof(sdbuffer);  // Force buffer reload
            tft.startWrite();            // Start new TFT transaction
          }

          for (col = 0; col < w; col++) {  // For each pixel...
            // Time to read more pixel data?
            if (buffidx >= sizeof(sdbuffer)) {  // Indeed
              tft.endWrite();                   // End TFT transaction
              bmpFile.read(sdbuffer, sizeof(sdbuffer));
              buffidx = 0;       // Set index to beginning
              tft.startWrite();  // Start new TFT transaction
            }

            // Convert pixel from BMP to TFT format, push to display
            b = sdbuffer[buffidx++];
            g = sdbuffer[buffidx++];
            r = sdbuffer[buffidx++];
            tft.pushColor(tft.color565(r, g, b));

          }              // end pixel
        }                // end scanline
        tft.endWrite();  // End last TFT transaction
                         //     Serial.print(F("Loaded in "));
                         //     Serial.print(millis() - startTime);
                         //     Serial.println(" ms");
      }                  // end goodBmp
    }
  }

  bmpFile.close();
  if (!goodBmp) { 
     Serial.println(F("BMP format not recognized."));
     tft.fillScreen(MYBLACK);
     tft.setRotation(1);
     tft.setCursor(5, 5);
    //tft.setTextSize(2);
     tft.setTextColor(MYWHITE);
     tft.print("Error Bitmap file: ");
     tft.print(filename);
    }
}

// These read 16- and 32-bit types from the SD card file.
// BMP data is stored little-endian, Arduino is little-endian too.
// May need to reverse subscript order if porting elsewhere.

uint16_t read16(File &f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read();  // LSB
  ((uint8_t *)&result)[1] = f.read();  // MSB
  return result;
}

uint32_t read32(File &f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read();  // LSB
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read();  // MSB
  return result;
}