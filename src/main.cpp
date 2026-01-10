#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>
#include <AsyncTCP.h>
#include <LittleFS.h>
#include <SPI.h>
#include <SD.h>
#include <sd_defines.h>
#include <sd_diskio.h>
#include <HX711.h>




//Serever ID

const char* ssid = "Stand Pruebas";
const char* password = "AltitudeRocketry";


TaskHandle_t MeasureHandle = NULL;
TaskHandle_t TransmitHandle = NULL;

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

HX711 scale;
File myFile;

#define SD_MISO_PIN 19
#define SD_MOSI_PIN 23
#define SD_SCK_PIN 18
#define SD_CS_PIN 5

#define CONTPIN 2

const uint8_t LOADCELL_DOUT_PIN = 32;  //Data pin for hx711
const uint8_t LOADCELL_SCK_PIN = 33;   //sck pin for hx711

bool cont = false;
bool hx_state;
bool sd_state;

bool TestFireActive = false;
bool continuityPin;

float thrust;
float impulse;
uint32_t testStartTime = millis();
uint32_t now;
uint32_t dt;
uint8_t MessageTime;

uint32_t lastSampleTime;
uint32_t currentTestTime;
// uint32_t instance;
int32_t calibrationValue;
int32_t KnownWeight;
String filename;


// put function declarations here:



String test(){
  if (!SD.begin(SD_CS_PIN)) {
    sd_state = false;
    hx_state = false;
  }
  else{
    
    if(SD.exists(filename)){
      Serial.println("SD FILE ALREADY EXISTS");
      sd_state = true;
    }
    else{
      myFile = SD.open("/" + filename + ".txt", FILE_APPEND);

      if(!myFile){
        sd_state = false;
        Serial.print("Not working");
      }
      else{
        myFile.println("timestamp, thrust, continuity");
        myFile.close();
        sd_state = true;
      }
    }

    if(scale.is_ready()){
          hx_state = true;
    }else{
          hx_state = false;
    }
        
  }

  return "{\"SD\":" + String(sd_state) + ", \"HX711\":" + String(hx_state) + "}";

}



void dataStore(){
  //SD card must me < 32 gb and formated FAT32
  myFile = SD.open("/" + filename + ".txt", FILE_APPEND);
  if(myFile){
    myFile.print(currentTestTime);
    myFile.print(",");
    myFile.println(thrust,3);
    myFile.close();
  }
  else{
    Serial.print("SD ERROR");
    Serial.print(";");
    myFile.close();
    sd_state = false;
  }
}

void measureTask(void * pvParameters){
  while(1){
    
    if(TestFireActive && scale.is_ready()){
      continuityPin = digitalRead(CONTPIN);
      
      now = millis();
      dt = now - lastSampleTime;
      currentTestTime = now - testStartTime;

      thrust = scale.get_units()/1000; 
      impulse += thrust * 9.81 * dt;

      // Serial.print("Time: ");
      // Serial.println(currentTestTime);
      // Serial.print("Thrust: ");
      // Serial.println(thrust);

      dataStore();
  }

    //   if(dt >= 50){
    //   String  data = "{\"thrust\":" + String(thrust) +  ",\"time\":" + String(currentTestTime) + ",\"cont\":" + String(continuityPin) + "}";
    //   ws.textAll(data);
    //   lastSampleTime = now;
    // }
      
      vTaskDelay(4/portTICK_PERIOD_MS);
  }

}

void DataTransmission(void * pvParameters){
  while(1){
      char json[128];
      snprintf(json, sizeof(json), 
      // "{\"thrust\":%.3f,\"time\":" + String(currentTestTime) + ",\"impulse\":" + String(impulse) + ",\"cont\":" + String(continuityPin) + "}"
        "{\"thrust\":%.3f,\"time\":%lu,\"impulse\":%.3f,\"cont\":%d}",
          thrust, currentTestTime, impulse, continuityPin);    
      
      if(TestFireActive){
        ws.textAll(json);
      }

      vTaskDelay(50 / portTICK_PERIOD_MS);
    
  }
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  pinMode(SD_CS_PIN, OUTPUT);
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale();
  scale.tare();
  digitalWrite(SD_CS_PIN, HIGH);


  xTaskCreatePinnedToCore(measureTask, "measureTask", 4096, NULL, 5, &MeasureHandle, 1);
  xTaskCreatePinnedToCore(DataTransmission, "Transmit Data", 4096, NULL, 2, &TransmitHandle, 1);

  Serial.println("Setting Access Point");
  if(!LittleFS.begin()){
    LittleFS.format();
    LittleFS.begin();
  }
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);

  Serial.println("");
  Serial.println("ESP32 WIFI Access Point set up!");

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP: ");
  Serial.println(IP);





  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
  // server.onNotFound([](AsyncWebServerRequest *request) {
  //   if (request->method() == HTTP_GET && request->url() == "/")
  //     request->send(LittleFS, "/index.html", "text/html");
  //   else
  //     request->send(404);
  // });

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/index.html", "text/html");
  });


  // server.on("/cont", HTTP_GET, [](AsyncWebServerRequest *request) {
  //   bool continuityPin = digitalRead(CONTPIN);

  //   request->send(200, "application/json", "{\"cont\":" + String(continuityPin) + "}");
  // });


  // server.on("/thrust", HTTP_GET, [](AsyncWebServerRequest *request){
  //   if(request->hasParam("FIRE")){
  //     if(!TestFireActive){
  //       testStartTime = millis();
  //       lastSampleTime = testStartTime;
  //       scale.set_scale(calibrationValue);
  //       TestFireActive = true;
  //     }
  //   }




  //   if(TestFireActive && scale.is_ready()){

  //     uint32_t now = millis();
  //     uint32_t dt = now - lastSampleTime;
  //     currentTestTime = now - testStartTime;


  //     thrust = scale.get_units()/1000; 

  //     Serial.print(currentTestTime, thrust);
  //     dataStore();


  //     String  data = "{\"thrust\":" + String(thrust) +  ",\"time\":" + String(currentTestTime) + "}";
  //     request->send(200, "application/json", data);
  //   }

    
  // });
  // All commands go through /cmd?action=xxx
  server.on("/cmd", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("action")) {
      request->send(400, "text/plain", "Missing action");
      return;
    }

    String action = request->getParam("action")->value();
    Serial.println(action);

    if (action == "set" && request->hasParam("value")) {
      // doSetCalibration();
      String val = request->getParam("value")->value();
      calibrationValue = val.toFloat();
      Serial.println(val);
      Serial.println("SET command received");
      String json = "{\"calibrationvalue\":" + val + "}";
      
      request->send(200, "application/json", json);
    }
    else if (action == "calibrate") {
      if(request->hasParam("weight")){
        String val = request->getParam("weight")->value();
        KnownWeight = val.toFloat(); 
        Serial.println(KnownWeight);
      }

      if(request->hasParam("task")){
        String task = request->getParam("task")->value();

        if(task == "tare"){
          scale.tare();
        }else if(task == "calibrate"){
          int32_t reading = scale.get_units(50);
          Serial.println(reading);
          Serial.println(KnownWeight);
          calibrationValue = reading/KnownWeight;
          Serial.print("Calibration Value: ");
          Serial.println(calibrationValue);

        }
        request->send(200, "text/plain", String(calibrationValue));
      }
      
      // bool ok = CalibrateScale();
      
      
    }
    else if (action == "test") {
      if(action == "test" && request->hasParam("name")){
        filename = request->getParam("name")->value();
        String res = test();
        Serial.println(res);
        request->send(200, "text/plain", res);
      }

    }
    else if (action == "ignite") {
      
      if(!TestFireActive){
        testStartTime = millis();
        lastSampleTime = testStartTime;
        scale.set_scale(calibrationValue);
        TestFireActive = false;
        Serial.println(calibrationValue);
        Serial.println("LESGO");
      }


    
      request->send(200, "text/plain", "Countdown started");
    }
    else if(action == "stop"){
      TestFireActive = false;

    }
    else if (action == "go"){
      TestFireActive = true;
    }

    else {
      request->send(400, "text/plain", "Unknown action");
    }
  });


  server.onNotFound([](AsyncWebServerRequest *request){
    request->send(204);
  });



  ws.onEvent([](AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){
    if(type == WS_EVT_CONNECT) Serial.printf("WS client #%u connected \n", client->id());
    else if(type == WS_EVT_DISCONNECT) Serial.printf("WS client #%u disconnected", client->id());
  });
  server.addHandler(&ws);


  server.begin();
  Serial.println("HTTP server started");



}


void loop() {
  // put your main code here, to run repeatedly:
  


}

