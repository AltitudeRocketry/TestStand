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

struct __attribute__((__packed__)) TelemetryPacket {
  float thrust;            // 4 bytes
  uint32_t currentTestTime;// 4 bytes
  uint8_t continuityPin;   // 1 byte
  bool sd_state;
};


String test(){
  if (!SD.begin(SD_CS_PIN)) {
    sd_state = false;
    hx_state = false;
  }
  else{
    
    if(SD.exists("/" + filename + ".txt")){
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
  }

  if(scale.is_ready()){
        hx_state = true;
  }else{
        hx_state = false;
  }
        
  

  return "{\"SD\":" + String(sd_state) + ", \"LoadCell\":" + String(hx_state) + "}";

}


void dataStore(){
  //SD card must me < 32 gb and formated FAT32
  myFile = SD.open("/" + filename + ".txt", FILE_APPEND);
  if(myFile){
    myFile.print(currentTestTime);
    myFile.print(",");
    myFile.println(thrust,3);
    myFile.print(",");
    myFile.println(continuityPin);
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

      // dataStore();
      if(myFile && sd_state){
        myFile.print(currentTestTime);
        myFile.print(",");
        myFile.println(thrust,3);
      }
      else{
          Serial.print("SD ERROR");
          Serial.print(";");
          sd_state = false;
      }
  }


    //   if(dt >= 50){
    //   String  data = "{\"thrust\":" + String(thrust) +  ",\"time\":" + String(currentTestTime) + ",\"cont\":" + String(continuityPin) + "}";
    //   ws.textAll(data);
    //   lastSampleTime = now;
    // }
      
      vTaskDelay(4/portTICK_PERIOD_MS);
  }

}

// void DataTransmission(void * pvParameters) {
//   while (1) {
//     // 1. Regularly clean up dead WebSocket connections from memory
//     ws.cleanupClients();

//     // 2. Only attempt transmission if a test is firing AND clients are ready
//     if (TestFireActive && ws.count() > 0 && ws.availableForWriteAll()) {
//       char json[128];
//       snprintf(json, sizeof(json), 
//         "{\"thrust\":%.3f,\"time\":%lu,\"impulse\":%.3f,\"cont\":%d}",
//         thrust, currentTestTime, impulse, continuityPin); 
      
//       Serial.print("Transmitting");
//       ws.textAll(json);
//     }

//     // 3. Keep transmission rate at 20Hz (50ms). 
//     // If packet overflow persists, change 50 to 100 (10Hz).
//     vTaskDelay(50 / portTICK_PERIOD_MS);
//   }
// }

void DataTransmission(void * pvParameters) {
  TelemetryPacket packet;
  while (1) {
    if (TestFireActive && ws.count() > 0 ) {

      packet.thrust = thrust;
      packet.currentTestTime = currentTestTime;
      packet.continuityPin = continuityPin;
      packet.sd_state = sd_state;

      ws.binaryAll((uint8_t*)&packet, sizeof(TelemetryPacket));
      // for (size_t i = 1; i < ws.count(); i++){
      //   AsyncWebSocketClient *client = ws.client(i);
        
      //   // SAFETY CHECKS: Must check for NULL pointer before checking queue!
      //   if (client != NULL && client->status() == WS_CONNECTED) {
          
      //     // Skip frame if TCP queue is full (prevents library auto-disconnect)
      //     if (!client->queueIsFull()) {
      //       char json[128];
      //       snprintf(json, sizeof(json), 
      //         "{\"thrust\":%.3f,\"time\":%lu,\"impulse\":%.3f,\"cont\":%d}",
      //         thrust, currentTestTime, impulse, continuityPin);
  
      //       client->text(json); // Use client->text instead of textAll
      //     }
      //   }
      // }
      // }
      
      // char json[128];
      // snprintf(json, sizeof(json), 
      //   "{\"thrust\":%.3f,\"time\":%lu,\"cont\":%d}",
      //   thrust, currentTestTime, continuityPin);

      // Thread-safe client transmission using count & client() lookup
// 3. Thread-safe loop through all active clients
        // AsyncWebSocketClient *client = ws.client(1);
      
        
        // // Ensure the client actually exists and is fully connected
        // if (client != nullptr && client->status() == WS_CONNECTED) {
          
        //   // 4. THE FIX: Check the actual message queue limit, not just RAM.
        //   if (client->queueIsFull()) {
        //     // TCP stack is lagging behind! 
        //     // We INTENTIONALLY DO NOTHING and drop this frame.
        //     // This prevents the buffer overflow and keeps the connection alive.
        //     Serial.print("yes");
        //   } else {
        //     // Safe to send
        //     client->text(json);
        //   }
        }
        
        
        // ws.textAll(json);
      
    // vTaskDelay(50 / portTICK_PERIOD_MS); // 20 Hz
    vTaskDelay(pdMS_TO_TICKS(50));
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
  xTaskCreatePinnedToCore(DataTransmission, "Transmit Data", 8192, NULL, 2, &TransmitHandle, 1);

  Serial.println("Setting Access Point");
  // if(!LittleFS.begin()){
  //   LittleFS.format();
  //   LittleFS.begin();
  // }
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);

  Serial.println("");
  Serial.println("ESP32 WIFI Access Point set up!");

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP: ");
  Serial.println(IP);


ws.onEvent([](AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len) { 
switch (type){
  case WS_EVT_CONNECT:
    Serial.printf("WS client #%u connected \n", client->id());
    client->setCloseClientOnQueueFull(false);
    break;
  case WS_EVT_DISCONNECT:
    Serial.printf("[WS] Client #%u disconnected! Reason code: %u\n", client->id(), arg);
    break;
  
  case WS_EVT_DATA:
   
    AwsFrameInfo * info = (AwsFrameInfo*)arg;
          if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {

            String message = String((char*)data, len);
            Serial.print("WS Received: ");
            Serial.println(message);

            if(message.startsWith("CMD_SET:")){
              String valStr = message.substring(8);
              calibrationValue = valStr.toFloat();
              Serial.print(calibrationValue);
            }
            else if (message.startsWith("CMD_CALIBRATE:")){
              String valStr = message.substring(14);
              KnownWeight = valStr.toFloat();
              Serial.print(KnownWeight);

            }
            else if(message.startsWith("CMD_TEST:")){
              filename = message.substring(9);
              String res = test();
              Serial.println(res);
              client->text(res);
            }
            else if(message.startsWith("CMD_MEASURE")){
              if(!TestFireActive){
                myFile = SD.open("/" + filename + ".txt", FILE_APPEND);
                testStartTime = millis();
                lastSampleTime = testStartTime;
                scale.set_scale(calibrationValue);
                TestFireActive = true;
                Serial.println("Ignition Armed, awaiting Countdown GO.");
              }
              client->text("{\"status\":\"armed\"}");
            }
            else if(message.startsWith("CMD_GO:")){
              // TestFireActive = true;
            client->text("{\"status\":\"firing\"}");
            }
            else if(message.startsWith("CMD_STOP:")){
              TestFireActive = false;
              if(myFile){
                myFile.close();
              }
              client->text("{\"status\":\"stopped\"}");
            }
          }
          break;

  }
});

server.addHandler(&ws);
server.begin();
Serial.println("HTTP & WebSocket Server Started");

}

//   server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
//   // server.onNotFound([](AsyncWebServerRequest *request) {
//   //   if (request->method() == HTTP_GET && request->url() == "/")
//   //     request->send(LittleFS, "/index.html", "text/html");
//   //   else
//   //     request->send(404);
//   // });

//   server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
//     request->send(LittleFS, "/index.html", "text/html");
//   });


//   // server.on("/cont", HTTP_GET, [](AsyncWebServerRequest *request) {
//   //   bool continuityPin = digitalRead(CONTPIN);

//   //   request->send(200, "application/json", "{\"cont\":" + String(continuityPin) + "}");
//   // });


//   // server.on("/thrust", HTTP_GET, [](AsyncWebServerRequest *request){
//   //   if(request->hasParam("FIRE")){
//   //     if(!TestFireActive){
//   //       testStartTime = millis();
//   //       lastSampleTime = testStartTime;
//   //       scale.set_scale(calibrationValue);
//   //       TestFireActive = true;
//   //     }
//   //   }




//   //   if(TestFireActive && scale.is_ready()){

//   //     uint32_t now = millis();
//   //     uint32_t dt = now - lastSampleTime;
//   //     currentTestTime = now - testStartTime;


//   //     thrust = scale.get_units()/1000; 

//   //     Serial.print(currentTestTime, thrust);
//   //     dataStore();


//   //     String  data = "{\"thrust\":" + String(thrust) +  ",\"time\":" + String(currentTestTime) + "}";
//   //     request->send(200, "application/json", data);
//   //   }

    
//   // });
//   // All commands go through /cmd?action=xxx
//   server.on("/cmd", HTTP_GET, [](AsyncWebServerRequest *request) {
//     if (!request->hasParam("action")) {
//       request->send(400, "text/plain", "Missing action");
//       return;
//     }

//     String action = request->getParam("action")->value();
//     Serial.println(action);

//     if (action == "set" && request->hasParam("value")) {
//       // doSetCalibration();
//       String val = request->getParam("value")->value();
//       calibrationValue = val.toFloat();
//       Serial.println(val);
//       Serial.println("SET command received");
//       String json = "{\"calibrationvalue\":" + val + "}";
      
//       request->send(200, "application/json", json);
//     }
//     else if (action == "calibrate") {
//       if(request->hasParam("weight")){
//         String val = request->getParam("weight")->value();
//         KnownWeight = val.toFloat(); 
//         Serial.println(KnownWeight);
//       }

//       if(request->hasParam("task")){
//         String task = request->getParam("task")->value();

//         if(task == "tare"){
//           scale.tare();
//         }else if(task == "calibrate"){
//           int32_t reading = scale.get_units(50);
//           Serial.println(reading);
//           Serial.println(KnownWeight);
//           calibrationValue = reading/KnownWeight;
//           Serial.print("Calibration Value: ");
//           Serial.println(calibrationValue);

//         }
//         request->send(200, "text/plain", String(calibrationValue));
//       }
      
//       // bool ok = CalibrateScale();
      
      
//     }
//     else if (action == "test") {
//       if(action == "test" && request->hasParam("name")){
//         filename = request->getParam("name")->value();
//         String res = test();
//         Serial.println(res);
//         request->send(200, "text/plain", res);
//       }

//     }
//     else if (action == "ignite") {
      
//       if(!TestFireActive){
//         testStartTime = millis();
//         lastSampleTime = testStartTime;
//         scale.set_scale(calibrationValue);
//         TestFireActive = false;
//         Serial.println(calibrationValue);
//         Serial.println("LESGO");
//       }


    
//       request->send(200, "text/plain", "Countdown started");
//     }
//     else if(action == "stop"){
//       TestFireActive = false;

//     }
//     else if (action == "go"){
//       TestFireActive = true;
//     }

//     else {
//       request->send(400, "text/plain", "Unknown action");
//     }
//   });


//   server.onNotFound([](AsyncWebServerRequest *request){
//     request->send(204);
//   });



//   ws.onEvent([](AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){
//     if(type == WS_EVT_CONNECT) Serial.printf("WS client #%u connected \n", client->id());
//     else if(type == WS_EVT_DISCONNECT) Serial.printf("WS client #%u disconnected", client->id());
//   });
//   server.addHandler(&ws);


//   server.begin();
//   Serial.println("HTTP server started");



// }


void loop(){}

