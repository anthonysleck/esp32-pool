/*
   ESP32 Pool Controller
   Description:
   -Coming Soon!
   Notes:
   -Coming Soon!
   Components:
    -ESP32-WROOM-32U
    -DS18B20
    -KY-019 5V One Channel Relay
    -4.7K ohm resistor
    -x3 1K ohm resistor
    -3mm dia blue LED
    -3mm dia green LED
    -x2 3 Pin 2.54mm Pitch PCB Screw Terminal Block Connector
    -2 Pin 2.54mm Pitch PCB Screw Terminal Block Connector
    -x2 19 pin female header
    -Custom PCB
   Connections:
   -Coming Soon!
   Contact Info:
   email - anthony.sleck@gmail.com
   web - sleckconsulting.com
   github - https://github.com/anthonysleck
   Changelog:
   0.01 - new code
*/

//inlcudes
#include <DallasTemperature.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <OneWire.h>
#include <Update.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "credentials.h"
#include "webpages.h"

//constants-variables
const String ver = "ESP Pool Controller - v0.01";
const char *host = "ESP32-POOL"; //change to your desired hostname
#define ONE_WIRE_BUS 2
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
const char* PARAM_INPUT_1 = "relay";
const char* PARAM_INPUT_2 = "state";
#define relayPin 15
#define RELAY_NO false
#define relayLEDPin 13
AsyncWebServer server(80);
size_t content_len;


String readDSTemperatureF() {
  //call sensors.requestTemperatures() to issue a global temperature and Requests to all devices on the bus
  sensors.requestTemperatures();
  float tempF = sensors.getTempFByIndex(0);

  if (int(tempF) == -196) {
    Serial.println("Failed to read from DS18B20 sensor");
    return "--";
  } else {
    Serial.print("Temperature Fahrenheit: ");
    Serial.println(tempF);
  }
  return String(tempF);
}

String tempProcessor(const String& var) {
  if (var == "TEMPERATUREF") {
    return readDSTemperatureF();
  }
  return String();
}

String relayProcessor(const String& var){
  if(var == "BUTTONPLACEHOLDER"){
    String button ="";
      String relayStateValue = relayState();
      button+= "<h4>Power Switch</h4><label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"" + String() + "\" "+ relayStateValue +"><span class=\"slider\"></span></label>";
    return button;
  }
  return String();
}

String relayState(){
  if(RELAY_NO){
    if(digitalRead(relayPin)){
      return "";
    }
    else {
      return "checked";
    }
  }
  else {
    if(digitalRead(relayPin)){
      return "checked";
    }
    else {
      return "";
    }
  }
  return "";
}

void rootServer(){
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/html", index_html);
  });

  server.on("/temp", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/html", temp_html, tempProcessor);
  });
  
  server.on("/temperaturef", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", readDSTemperatureF().c_str());
  });

  server.on("/relay", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/html", relay_html, relayProcessor);
  });

  server.on("/updateData", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String inputMessage;
    String inputParam;
    String inputMessage2;
    String inputParam2;
    if (request->hasParam(PARAM_INPUT_1) & request->hasParam(PARAM_INPUT_2)) {
      inputMessage = request->getParam(PARAM_INPUT_1)->value();
      inputParam = PARAM_INPUT_1;
      inputMessage2 = request->getParam(PARAM_INPUT_2)->value();
      inputParam2 = PARAM_INPUT_2;
        digitalWrite(relayPin, !inputMessage2.toInt());
        digitalWrite(relayLEDPin, !inputMessage2.toInt());
      }
    else {
      inputMessage = "No message sent";
      inputParam = "none";
    }
    Serial.println(inputMessage + inputMessage2);
    request->send(200, "text/plain", "OK");
  });

  //start server
  server.begin();
}

void handleUpdate(AsyncWebServerRequest *request) {
  char* html = "<form method='POST' action='/doUpdate' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";
  request->send(200, "text/html", html);
}

void handleDoUpdate(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final)  {
  if (!index){
    Serial.println("Update");
    content_len = request->contentLength();
    #ifdef ESP8266
    int cmd = (filename.indexOf("spiffs") > -1) ? U_FS : U_FLASH;
    #else
    int cmd = (filename.indexOf("spiffs") > -1) ? U_SPIFFS : U_FLASH;
    #endif
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, cmd)) {
      Update.printError(Serial);
    }
  }

  if (Update.write(data, len) != len) {
    Update.printError(Serial);
  }

  if (final) {
    AsyncWebServerResponse *response = request->beginResponse(302, "text/plain", "Please wait while the device reboots");
    response->addHeader("Refresh", "20");  
    response->addHeader("Location", "/");
    request->send(response);
    if (!Update.end(true)){
      Update.printError(Serial);
    } else {
      Serial.println("Update complete");
      Serial.flush();
      ESP.restart();
    }
  }
}

void printProgress(size_t prg, size_t sz) {
  Serial.printf("Progress: %d%%\n", (prg*100)/content_len);
}

boolean webInit() {
  server.on("/firmware", HTTP_GET, [](AsyncWebServerRequest *request){handleUpdate(request);});
  server.on("/doUpdate", HTTP_POST,
    [](AsyncWebServerRequest *request) {},
    [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data,
                  size_t len, bool final) {handleDoUpdate(request, filename, index, data, len, final);}
  );
  server.onNotFound([](AsyncWebServerRequest *request){request->send(404);});
  server.begin();
  Update.onProgress(printProgress);
}

void WiFiConnect(){
  //start WiFi
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.print(".");
  }

  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void setup()
{
  //start serial
  Serial.begin(115200);

  //print sketch information
  Serial.println("Created by Anthony Sleck");
  Serial.println("Email at anthony.sleck@gmail.com");
  Serial.println(ver);
  Serial.println("github - https://github.com/anthonysleck");

  //start WiFi
  WiFiConnect();

  //start sensors
  sensors.begin();

  //set all relays to off when the program starts - if set to Normally Open (NO), the relay is off when you set the relay to HIGH
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);
  pinMode(relayLEDPin, OUTPUT);
  digitalWrite(relayLEDPin, LOW);

  //start web server for updating
  rootServer();
  MDNS.begin(host);
  if(webInit()) MDNS.addService("http", "tcp", 80);
  Serial.printf("Ready! To update firmware, open http://%s.local/firmware in your browser!\n", host);
  Serial.printf("Otherwise to open the root page, open http://%s.local in your browser!\n", host);
}

void loop(void){
  //this is a sample code to output free heap every 5 seconds; this is a cheap way to detect memory leaks
  static unsigned long last = millis();
  if (millis() - last > 5000) {
    last = millis();
    Serial.printf("[MAIN] Free heap: %d bytes\n", ESP.getFreeHeap());
  }
}