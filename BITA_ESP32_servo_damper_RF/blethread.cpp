#include "blethread.h"
#include "common_functions.h"
#include "EEPROM.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ArduinoJson.h>

#define IND_PIN 25

extern String deviceinfo;
extern String macaddress;
extern int device_type;
extern String devversion;
extern String myID;
extern long disconnection_counter;
extern long switchblecounter;
extern String devicename;
extern String ssid;
extern String password;
extern bool deviceConnected;
extern int wifi_channel;

extern String slave_mac_address;
extern String ble_slave_mac_address;

int modeAddr = 0; 
int wifiAddr = 100;

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      BLEDevice::startAdvertising();
    };
    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};
class MyCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic){
    std::string value = pCharacteristic->getValue();

    if(value.length() > 0){
      if(strstr (value.c_str(), "getinfo")){
        Serial.println("Get Info Called\n");  
        //---------------------------------------------------------------------
        //UPDATE DEVICE INFO IF CHANGED, BEFORE SENDING. ALSO UPDATE DC TIMER
        //---------------------------------------------------------------------
        deviceinfo="";
        DynamicJsonDocument jsonBufferinfo(200);
        jsonBufferinfo["mac_address"] = macaddress;
        jsonBufferinfo["slave_mac_address"] = "";
        jsonBufferinfo["type"] = device_type;
        jsonBufferinfo["vers"] = devversion;
        jsonBufferinfo["id"]=myID; 
        jsonBufferinfo["disconnection_counter"]=disconnection_counter; 
        jsonBufferinfo["device_timeout_counter"]=switchblecounter; 
        jsonBufferinfo["wifi_channel"]=wifi_channel;
        jsonBufferinfo["wifissid"]=ssid;
        jsonBufferinfo["wifipassword"]=password;
        serializeJson(jsonBufferinfo, deviceinfo);
        //---------------------------------------------------------------------
        pCharacteristic->setValue(deviceinfo.c_str());
        pCharacteristic->notify();
      }
      else if(strstr (value.c_str(), "startwifi")){
        Serial.println("Restarting BLE!\n");  
        delay(500);
        EEPROM.write(52, 0);EEPROM.commit();
        ESP.restart();
      }
      else if(strstr (value.c_str(), "wifi")){
        Serial.println("credentials received, sorting out...!\n");
        String mystr = getValue(value.c_str(),':',1);
        writeString(wifiAddr,mystr);
        Serial.print("WIFI : ");Serial.println(mystr);
        ssid = getValue(mystr,',',0);
        password = getValue(mystr,',',1);
        ble_slave_mac_address = getValue(mystr,',',2);
        if(ble_slave_mac_address != ""){
          slave_mac_address = ble_slave_mac_address;
        }
        Serial.print("SLAVE_MAC_BLU: ");Serial.println(slave_mac_address);
        // ccx2 = slave_mac_address.c_str();
        // sscanf(ccx2, "%x/%x/%x/%x/%x/%x%*c",
        //             &broadcastAddress[0], broadcastAddress[1], &broadcastAddress[2],
        //             &broadcastAddress[3], &broadcastAddress[4], &broadcastAddress[5]);
        
      }
    }
  }
  void writeString(int add, String data){
    int _size = data.length();
    for(int i=0; i<_size; i++){
      EEPROM.write(add+i, data[i]);
    }
    EEPROM.write(add+_size, '\0');
    EEPROM.commit();
  }
};
void bleTask(){
    delay(1000);
    digitalWrite(IND_PIN,HIGH);
    delay(125);
    digitalWrite(IND_PIN,LOW);
    delay(125);
    digitalWrite(IND_PIN,HIGH);
    delay(125);
    digitalWrite(IND_PIN,LOW);
    delay(125);
    digitalWrite(IND_PIN,HIGH);
    delay(125);
    digitalWrite(IND_PIN,LOW);
    // Create the BLE Device
    BLEDevice::init(devicename.c_str());
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());
    BLEService *pService = pServer->createService(SERVICE_UUID);
    pCharacteristic = pService->createCharacteristic(
                        CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ|BLECharacteristic::PROPERTY_WRITE|BLECharacteristic::PROPERTY_NOTIFY|BLECharacteristic::PROPERTY_INDICATE
                      );
  
    pCharacteristic->setCallbacks(new MyCallbacks());
    // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml
    // Create a BLE Descriptor
    pCharacteristic->addDescriptor(new BLE2902());
    pService->start();
  
    // Start advertising
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);  // set value to 0x00 to not advertise this parameter
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
    Serial.println("starting BLE advertise");
}

void blestop(){
  delay(1000);
  digitalWrite(IND_PIN,HIGH);
  delay(1000);
  digitalWrite(IND_PIN,LOW);
  BLEDevice::init(devicename.c_str());
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
                        CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ|BLECharacteristic::PROPERTY_WRITE|BLECharacteristic::PROPERTY_NOTIFY|BLECharacteristic::PROPERTY_INDICATE
                      );
  
  pCharacteristic->setCallbacks(new MyCallbacks());
  // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml
  // Create a BLE Descriptor
  pCharacteristic->addDescriptor(new BLE2902());
  pService->stop();
  Serial.println("BLE Server Stopped!");

  // Turn off the Bluetooth radio
  btStop();
  Serial.println("Bluetooth Radio Stopped!");
}
