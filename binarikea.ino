#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <WiFiAP.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <bitset>
#include <array>
#include "time.h"

using namespace std;

struct currTime{
  int hours;
  int minutes;
};

AsyncWebServer server(80);

const char * ssid = "Prancing_Pony";
const char * wifipw = "Aragorn!";

#define LED_TYPE NEO_GRB+NEO_KHZ800

std::array<bool, 4> decimalToBinary(int num);

Adafruit_NeoPixel strips[] = {
  Adafruit_NeoPixel(4, 19, LED_TYPE),
  Adafruit_NeoPixel(4, 18, LED_TYPE),
  Adafruit_NeoPixel(4, 17, LED_TYPE),
  Adafruit_NeoPixel(4, 16, LED_TYPE),
};

uint32_t WHITE = strips[0].Color(255,255,255);
uint32_t BLACK = strips[0].Color(0,0,0);
uint32_t RED = strips[0].Color(255,0,0);
uint32_t YELLOW = strips[0].Color(255,255,0);
uint32_t GREEN = strips[0].Color(0,255,0);
uint32_t CYAN = strips[0].Color(0,255,255);
uint32_t BLUE = strips[0].Color(0,0,255);
uint32_t MAGENTA = strips[0].Color(255,0,255);
uint32_t bg = BLACK;
uint32_t fg = strips[0].Color(0, 187, 255);

std::array<std::array<bool, 4>, 4> blank = {
  decimalToBinary(0),
  decimalToBinary(0),
  decimalToBinary(0),
  decimalToBinary(0)
};

bool timerActive = false;
bool timerFired = false;

int direction = 1;
int last_direction = 1;
double pitch = 0;
double roll = 0;

long defaultTimerMillis = 30 *60 * 1000;
long lastClockCheckMillis = -1;
long lastSensorPollMillis = -1;
long lastSandWipe = -1;
long timerStartedMillis = -1;
long lastTimerUpdateMillis = -1;
// long lightTimerStartedMillis = -1;

bool iCanHazSensor = false;

Adafruit_MPU6050 mpu;

void setup() {
  Serial.begin(115200);
  Serial.println("Binarikea Timer");
  Serial.println(" - Begin setup");
  Serial.println(" - Starting sensor");
  if(!mpu.begin()){
    iCanHazSensor = false;
    Serial.println("\tSensor init failed, will have one function");
  }else{
    iCanHazSensor = true;
    Serial.println("\tSensor inited");
  }
  Serial.println(" - Starting leds");
  for(int x = 0; x < 4; x++){
    strips[x].begin();
    strips[x].setBrightness(50);
    Serial.print("\tStrip ");
    Serial.print(x+1);
    Serial.println(" started");
    colorWipe(&strips[x], WHITE, 50);
    delay(700);
    colorWipe(&strips[x], BLACK, 50);
  }
  displayState(0, blank, YELLOW, BLACK);
  int wifiCode = startWifi(); // 0 is wifi good; 1 is wifi failed, ap good; -1 is both failed;
  setupServer();
  initTime("PST8PDT,M3.2.0,M11.1.0");
  // printLocalTime();
  std::array<std::array<bool, 4>, 4> status = {
    decimalToBinary(0),
    decimalToBinary(0),
    decimalToBinary(0),
    decimalToBinary(0)
  };

  if(!iCanHazSensor){
    status[0] = decimalToBinary(15);
    status[1] = decimalToBinary(15);
  }
  if(wifiCode == 0){
    status[2] = decimalToBinary(15);
    status[3] = decimalToBinary(15);
  }else if(wifiCode == 1){
    status[2] = decimalToBinary(12);
    status[3] = decimalToBinary(12);
  }else if(wifiCode == -1){
    status[2] = decimalToBinary(0);
    status[3] = decimalToBinary(0);
  }
  displayState(0, status, GREEN, RED);
  delay(500);
}

// 1 is up
// 2 is right
// 3 is down
// 4 is left


// LOOP

void loop() {
  if(sensorCanUpdate()){
    updateSensor();
  }
  // Mode init logic
  if(direction != last_direction){
    switch(direction){
      case 1:
        Serial.println("Clock setup");
        // Normal clock
        lastClockCheckMillis = -1;
        break;
      case 2:
        // Time
        if(timerActive)
          break;
        else
          activateTimer();
        break;
      case 3:
        // Sand (Color wipe)
        sand();
        break;
      case 4:
        setStrips(RED);
        break;
    }
  }
  // Update logic
  switch(direction){
    case 1:
      doClockUpdateIfNeeded();
      break;
    case 2:
      checkTimer();
      break;
    case 3:
      // Sand;
      repeatSand();
      break;
    case 4:
      // Light timer; not implemented
      break;
  }
  last_direction = direction;
}

// END OF LOOP



void activateTimer(){
  flashStrips(MAGENTA, BLACK, 200);
  delay(90);
  timerStartedMillis = millis();
  timerActive = true;
}

void stopTimer(){
  timerActive = false;
  timerFired = false;
}





// Mode logic functions


void sand(){
  setStrips(BLACK);
  delay(200);
  colorWipeAll(fg, 50);
  delay(1000);
  colorWipeAll(BLACK, 50);
}
void repeatSand(){
  if(millis() - lastSandWipe >= 1000 || lastSandWipe == -1){
    sand();
  }
}

void checkTimer(){
  if(!timerActive) return;
  if(millis() - lastTimerUpdateMillis >= 1010){
    int timerRemainingSeconds = (defaultTimerMillis - (millis() - timerStartedMillis)) / 1000;
    int m = timerRemainingSeconds / 60;
    int m10 = m / 10;
    int m1 = m % 10;
    int s = timerRemainingSeconds % 60;
    int s10 = s / 10;
    int s1 = s % 10;
    std::array<std::array<bool, 4>, 4> cState = {
      decimalToBinary(m10),
      decimalToBinary(m1),
      decimalToBinary(s10),
      decimalToBinary(s1)
    };
    displayState(direction, cState, bg, fg);
    if(millis() - timerStartedMillis >= defaultTimerMillis){
      timerFired = true;
    }
    lastTimerUpdateMillis = millis();
  }
  if(timerFired){
    while(direction == last_direction){
      if(sensorCanUpdate){
        updateSensor();
        determineDirection();
      }
      flashStrips(YELLOW, MAGENTA, 400);
    }
    setStrips(BLACK);
    stopTimer();
  }
}

void doClockUpdateIfNeeded(){
  if(millis() - lastClockCheckMillis >= 60000 || lastClockCheckMillis == -1){
    Serial.println("Updating clock");
    currTime ctime = getTime();
    int h = ctime.hours;
    int h10 = h / 10;
    int h1 = h % 10;
    int m = ctime.minutes;
    int m10 = m / 10;
    int m1 = m % 10;
    std::array<std::array<bool, 4>, 4> cState = {
      decimalToBinary(h10),
      decimalToBinary(h1),
      decimalToBinary(m10),
      decimalToBinary(m1)
    };
    // uint32_t color = direction == 1 ? RED : 
    //     direction == 2 ? YELLOW :
    //     direction == 3 ? GREEN :
    //     BLUE;
    displayState(direction, cState, bg, fg);
    lastClockCheckMillis = millis();
  }
}






void displayState(int direction, std::array<std::array<bool, 4>, 4>& val, uint32_t bgColor, uint32_t fgColor){
  for(int x = 0; x < direction; x++){
    rotate(val);
  }
  if(direction % 2 == 0 && direction != 0) verticalFlip(val);
  else horizontalFlip(val);
  for(size_t x = 0; x < 4; x++){
    for(size_t y = 0; y < 4; y++){
      if(val[x][y]){
        // Serial.print("1 "); // We don't need this now that it's working properly
        strips[x].setPixelColor(y, fgColor);
      }else{
        // Serial.print("0 "); // Ditto
        strips[x].setPixelColor(y, bgColor);
      }
    }
    // Serial.println(""); // This is junk now
  }
  for(int x = 0; x < 4; x++)
    strips[x].show();
}

// Gryo interfacing

bool sensorCanUpdate(){
  return iCanHazSensor && (millis() - lastSensorPollMillis >= 300 || lastSensorPollMillis == -1);
}
void updateSensor(){
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  double pi = 3.141519265;
  double xAccel=a.acceleration.x;
  double yAccel=a.acceleration.y;
  double zAccel=a.acceleration.z;
  double localPitch=atan2(yAccel, zAccel); // some inverse trig or somethin idk lol
  double localRoll=atan2(xAccel, zAccel);  // arc tangent with respect to the quadrant the vector is in (gives angle from ratio)
  double pitchDeg=localPitch/(2*pi)*360; // Radians to degrees, 360rad over 2pi
  double rollDeg=localRoll/(2*pi)*360;
  roll = rollDeg;
  pitch = pitchDeg;
  // Serial.println("Sensor readings updated:");
  // Serial.print("\tPitch: ");
  // Serial.print(pitch);
  // Serial.print("\tRoll: ");
  // Serial.println(roll);
  determineDirection();
  lastSensorPollMillis = millis();
}
void determineDirection(){
  if(pitch >= -135 && pitch < -45){
    direction = 1;
  }else if(pitch >= -45 && pitch < 45){
    direction = 2;
  }else if(pitch >= 45 && pitch < 135){
    direction = 3;
  }else{
    direction = 4;
  }
  // Serial.print("New direction: ");
  // Serial.println(direction);
}


// LED Utils


void colorWipe(Adafruit_NeoPixel *strip, uint32_t color, int wait) {
  for(int i=0; i<strip->numPixels(); i++) { // For each pixel in strip...
    strip->setPixelColor(i, color);         //  Set pixel's color (in RAM)
    strip->show();                          //  Update strip to match
    delay(wait);                           //  Pause for a moment
  }
}
void colorWipeAll(uint32_t color, int wait){
    for(int x = 3; x >= 0; x--){
    strips[0].setPixelColor(x, color);
    strips[1].setPixelColor(x, color);
    strips[2].setPixelColor(x, color);
    strips[3].setPixelColor(x, color);
    for(int x = 0;x<4;x++)
      strips[x].show();
    delay(wait);
  }
}
void setStrips(uint32_t color){
  for(int x = 0; x < 4; x++){
    strips[0].fill(color,0,4);
    strips[1].fill(color,0,4);
    strips[2].fill(color,0,4);
    strips[3].fill(color,0,4);
  }
  for(int x = 0;x<4;x++)
    strips[x].show();
}
void flashStrips(uint32_t fg, uint32_t bg, int time){
  setStrips(fg);
  delay(time);
  setStrips(bg);
}
void flashStrips(uint32_t fg, uint32_t bg, int time, int count){
  for(int x = 0; x < count; x++){
    setStrips(fg);
    delay(time);
    setStrips(bg);
    delay(time);
  }
}




// Helper functions

void setTimezone(String timezone){
  Serial.printf("\t - Setting Timezone to %s\n",timezone.c_str());
  setenv("TZ",timezone.c_str(),1);  //  Now adjust the TZ.  Clock settings are adjusted to show the new local time
  tzset();
}

void initTime(String timezone){
  struct tm timeinfo;

  Serial.println(" - Setting up time");
  configTime(0, 0, "pool.ntp.org");    // First connect to NTP server, with 0 TZ offset
  if(!getLocalTime(&timeinfo)){
    Serial.println("\tFailed to obtain time");
    return;
  }
  Serial.println("\tGot the time from NTP");
  // Now we can set the real timezone
  setTimezone(timezone);
}

void printLocalTime(){
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time 1");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S zone %Z %z ");
  Serial.println(&timeinfo);
}

currTime getTime(){
  struct tm timeinfo;
  currTime ct;
  ct.hours = -1;
  ct.minutes = -1;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to get time");
    return ct;
  }
  char timeHour[3];
  strftime(timeHour,3, "%I", &timeinfo);
  char timeMinutes[3];
  strftime(timeMinutes,3,"%M",&timeinfo);

  ct.hours = atoi(timeHour);
  if(ct.hours > 12) ct.hours -= 12;
  ct.minutes = atoi(timeMinutes);
  return ct;
}

// std::array<bool,4> bitsetToArray(bitset *in){
//   Serial.print("Bitsetting:\n\t");
//   int len = 4;
//   std::array<bool, 4> arr;
//   for(int x = 0; x < len; x++){
//     // Serial.println(in[x]);
//     arr[x] = in[x];
//   }
//   return arr;
// }
// 
// int arrayLen(auto *array[]){
//   return sizeof(array) / sizeof(array[0]);
// }

int startWifi(){
  WiFi.begin(ssid, wifipw);
  Serial.print(" - Connecting Wifi\n\t");
  int status = 0;
  long wifiStartMillis = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStartMillis < 4000) {
    Serial.print(".");
    delay(500);
  }
  if(WiFi.status() != WL_CONNECTED){
    Serial.println("\tFailed to connect to wifi.");
    Serial.println(" - Starting AP");
    status = 1;
    if (!WiFi.softAP("BinarIKEA", "supersecure")) {
      Serial.println("\tFailed to start AP");
      status = -1;
    }else{
      Serial.println("\Started AP");
    }
  }else{ 
    Serial.print("\n - Wifi RSSI=");
    Serial.println(WiFi.RSSI());
    Serial.print(" - ");
    Serial.println(WiFi.localIP());
  }
  Serial.println(" - Starting mDNS repsonder");
  if(MDNS.begin("binarikea")){
    MDNS.addService("http", "tcp", 80);
    Serial.println("\tmDNS responder started");
  }else{
    Serial.println("\tmDNS failed");
  }
  return status;
}

void setupServer(){
  Serial.println(" - Starting web server");
  server.on("/timerDuration", HTTP_GET, [] (AsyncWebServerRequest *request) {
    //  /timerDuration?time=<seconds>
    if (request->hasParam("time")) {
      String stringVal = request->getParam("time")->value();
      defaultTimerMillis = stringVal.toInt() * 1000;
      timerStartedMillis = millis();
      lastTimerUpdateMillis = -1;
      Serial.print("\tSet timer length (seconds) to ");
      Serial.println(defaultTimerMillis/1000);
      request->send(200, "application/json", "{\"success\": true}");
    }else{
      request->send(500, "application/json", "{\"success\": false}");
    }
  });
  server.on("/stopTimer", HTTP_GET, [] (AsyncWebServerRequest *request){
    stopTimer();
    request->send(200, "application/json", "{\"success\": true}");
  });
  server.on("/fgColor", HTTP_GET, [] (AsyncWebServerRequest *request) {
    if(!request->hasParam("red") || !request->hasParam("green") || !request->hasParam("blue")){
      request->send(500, "text/plain", "insufficient data");
      return;
    }
    String rString = request->getParam("red")->value();
    String gString = request->getParam("green")->value();
    String bString = request->getParam("blue")->value();
    int red = rString.toInt();
    int green = gString.toInt();
    int blue = bString.toInt();
    fg = strips[0].Color(red, green, blue);
    lastClockCheckMillis = -1;
    lastTimerUpdateMillis = -1;
    request->send(200, "application/json", "{\"success\": true}");
  });
  server.on("/bgColor", HTTP_GET, [] (AsyncWebServerRequest *request) {
    if(!request->hasParam("red") || !request->hasParam("green") || !request->hasParam("blue")){
      request->send(500, "text/plain", "insufficient data");
      return;
    }
    String rString = request->getParam("red")->value();
    String gString = request->getParam("green")->value();
    String bString = request->getParam("blue")->value();
    int red = rString.toInt();
    int green = gString.toInt();
    int blue = bString.toInt();
    bg = strips[0].Color(red, green, blue);
    lastClockCheckMillis = -1;
    lastTimerUpdateMillis = -1;
    request->send(200, "application/json", "{\"success\": true}");
  });
  server.on("/brightness", HTTP_GET, [] (AsyncWebServerRequest *request) {
    if(!request->hasParam("brightness")){
      request->send(500, "text/plain", "insufficient data");
      return;
    }
    String bString = request->getParam("brightness")->value();
    int brightness = bString.toInt();
    for(int x = 0; x<4;x++){
      strips[x].setBrightness(brightness);
      strips[x].show();
    }
    lastClockCheckMillis = -1;
    lastTimerUpdateMillis = -1;
    request->send(200, "application/json", "{\"success\": true}");
  });
  server.on("/", HTTP_GET, [] (AsyncWebServerRequest *request) {
    request->send(200, "text/html", "<h1>Binarikea</h1>\n<p>/timerDuration?time=<em>seconds</em></p>\n<p>/[f|b]gColor?red=<em>redVal</em>&green=<em>greenVal</em>&blue=<em>blueVal</em></p>\n<p>/stopTimer</p>");
  });
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("\t Server started");
}

void setTime(int yr, int month, int mday, int hr, int minute, int sec, int isDst){
  struct tm tm;

  tm.tm_year = yr - 1900;   // Set date
  tm.tm_mon = month-1;
  tm.tm_mday = mday;
  tm.tm_hour = hr;      // Set time
  tm.tm_min = minute;
  tm.tm_sec = sec;
  tm.tm_isdst = isDst;  // 1 or 0
  time_t t = mktime(&tm);
  Serial.printf("\t - Setting time: %s", asctime(&tm));
  struct timeval now = { .tv_sec = t };
  settimeofday(&now, NULL);
}

std::array<bool, 4> decimalToBinary(int num) {
    std::array<bool, 4> binaryArray = {false, false, false, false};

    // Ensure that the number is in the 4-bit range (0 to 15)
    num = num & 0xF;  // Apply a mask to restrict the number to 4 bits (binary 0000-1111)

    // Fill the binary array with the binary representation
    for (int i = 3; i >= 0; --i) {
        binaryArray[i] = (num & 1);  // Extract the least significant bit
        num >>= 1;                   // Shift the number to the right
    }

    return binaryArray;
}

void rotate(std::array<std::array<bool, 4>, 4>& arr) {
    // Manually rotate the array 90 degrees clockwise.
    // We use the formula: (i, j) -> (j, 3 - i) for clockwise rotation.

    // First, copy the elements in their new positions
    bool temp[4][4];  // Temporary array for storing the rotated values
    
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            temp[j][3 - i] = arr[i][j];  // Mapping (i, j) -> (j, 3 - i)
        }
    }

    // Now copy the rotated values back into arr
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            arr[i][j] = temp[i][j];
        }
    }
}

void verticalFlip(std::array<std::array<bool, 4>, 4>& arr) {
    // Swap the rows vertically (flip over the horizontal axis)
    for (int i = 0; i < 2; ++i) {  // Only need to swap first half with the second half
        for (int j = 0; j < 4; ++j) {  // Loop over columns in each row
            std::swap(arr[i][j], arr[3 - i][j]);
        }
    }
}

void horizontalFlip(std::array<std::array<bool, 4>, 4>& arr) {
    // Flip the rows horizontally (reverse each row)
    for (int i = 0; i < 4; ++i) {  // Loop over all rows
        for (int j = 0; j < 2; ++j) {  // Only need to swap the first two elements with the last two
            std::swap(arr[i][j], arr[i][3 - j]);
        }
    }
}

/*
void rotate(std::array<std::array<bool, 4>, 4>& arr, int n) {
    // Normalize n to the range [0, 3]
    n = (n % 4 + 4) % 4; // Handling negative n values as well
    
    // Apply the appropriate number of rotations
    for (int i = 0; i < n; ++i) {
        rotate90Clockwise(arr);
    }
}
*/




// Web Server stuff

void handleNotFound(AsyncWebServerRequest *request) {
  // String message = "File Not Found\n\n";
  // message += "URI: ";
  // message += server.uri();
  // message += "\nMethod: ";
  // message += (server.method() == HTTP_GET) ? "GET" : "POST";
  // message += "\nArguments: ";
  // message += server.args();
  // message += "\n";

  // for (uint8_t i = 0; i < server.; i++) {
  //   message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  // }

  request->send(404, "text/plain", "Not found");
}
