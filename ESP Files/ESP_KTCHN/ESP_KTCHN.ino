#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "time.h"
#include <ArduinoJson.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>


// Bluetooth Variables
String beaconAddresses = "c8:48:9c:26:a3:f4";
int scanTime = 10; //In seconds
BLEScan* pBLEScan;


//WiFi Variables
const char* ssid = "BT-7XCK6F";
const char* password =  "urgcqY3H3XCPDv";
const char* serverName = "https://webhooks.mongodb-realm.com/api/client/v2.0/app/predictor-rjhbq/service/PushBLEData/incoming_webhook/webhook_ktchn?secret=SIoT";
StaticJsonDocument<500> doc;
int backlog = 0;
int timeStampStack[720];
int RSSIStack[720];

//Server used to find time
const char* ntpServer = "pool.ntp.org";

int awakeTime;
int delayTime;


//Function to deduce the RSSI signal strength of the FitBit
int pollBLEBeacon() {

  //Sets up ESP's bluetooth functions
  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan(); //Create new scan
  pBLEScan->setActiveScan(true); //Active scan get's results faster
  pBLEScan->setInterval(100); // Interval between scans
  pBLEScan->setWindow(99);

  //Scan's for BLE devices for a number of seconds defined by scanTime variable
  BLEScanResults foundDevices = pBLEScan->start(scanTime, false);

  //Goes through each device found and determine if it is the FitBit - if FitBit isn't found RSSI is set to -100.
  int becaonRSSI = -100;
  for (int i = 0; i < foundDevices.getCount(); i++) {
    
    BLEAdvertisedDevice device = foundDevices.getDevice(i);
    String deviceAddress = device.getAddress().toString().c_str();
    int rssi = device.getRSSI();
    if (deviceAddress == beaconAddresses) {
      becaonRSSI = rssi;}
  }

  
  pBLEScan->clearResults(); // delete results fromBLEScan buffer to release memory
  BLEDevice::deinit(); // Switches of bluetooth function as it can effect WiFi reliability
  
  //Returns single strenth or the value of -100 if device is not found
  return becaonRSSI;
}

//Function to push collected data instance to MongoDB server
int pushToDB(int becaonRSSI, int awakeTime) {

  timeStampStack[backlog] = awakeTime;
  RSSIStack[backlog] = becaonRSSI;

  int vla1 = timeStampStack[backlog];
  int vla2 = RSSIStack[backlog];
  //Serial.println(vla1);
  //Serial.println(vla2);

  //Connects to home WiFi network
  WiFi.begin(ssid, password);
  
  long connect_time = millis();
  while ((WiFi.status() != WL_CONNECTED) && ((millis() - connect_time) < 10000)) {
    delay(500);
    Serial.println("Connecting to WiFi..");}
 

  
  if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Connected to the WiFi network");
      
      for (int i = 0; i <= backlog; i++) { 
        
        //Saves the standardised awake minute and collected RSSI value to a JSON file
        doc["timeStampID"] = timeStampStack[i];
        doc["rssiKTCHN"] = RSSIStack[i];
        Serial.println(timeStampStack[i]);
        Serial.println(RSSIStack[i]);
        timeStampStack[i] == 0;
        RSSIStack[i] == 0;

        
        //Pushes JSON file using a post request to the mongoDB database using a webook
        HTTPClient http;
        http.begin(serverName);
        http.addHeader("Content-Type", "application/json");
        String json;
        serializeJson(doc, json);
        int httpResponseCode = http.POST(json);
        Serial.println(httpResponseCode);
      }

      //Returns Reponse Code
      return 1;


    }
    else {
      Serial.println("WiFi Connection Failed");
      backlog = backlog + 1;

      return 0;
    };
    
}

int timeTillNextPoll() {  
  configTime(0, 0, ntpServer);
  time_t now;
  struct tm timeinfo;
  
  if (!getLocalTime(&timeinfo)) {
    //Serial.println("Failed to obtain time");
    return(0);
  }
  
  time(&now);
  delayTime = (60-(now % 60))*1000;
  awakeTime = now+(delayTime/1000);

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  
  return delayTime;
}




void setup() {  
  Serial.begin(115200); //Enable UART on ESP32

  Serial.println("Running BLE Detection & POST Routine."); // Print Scanning
  pinMode(2, OUTPUT); //make BUILTIN_LED pin as output
  
  timeTillNextPoll();
  Serial.println(delayTime);

  Serial.println("Entering Deep Sleep");
  delay(delayTime);

    

  
}
void loop() {
  int beaconRSSI = pollBLEBeacon();
  int response = pushToDB(beaconRSSI, awakeTime);
  
  if (response == 1) {
    backlog = 0;
  }
  timeTillNextPoll();

  Serial.println("Entering Deep Sleep");
  delay(delayTime);
};