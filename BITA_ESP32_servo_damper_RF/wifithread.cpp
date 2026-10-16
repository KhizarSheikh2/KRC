#include "wifithread.h"
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "externvars.h"
#include "common_functions.h"
#include "EEPROM.h"
#include "time.h"
#include <WiFi.h>
#include <AsyncElegantOTA.h>

const char* ntpServer = "time.google.com";
const long  gmtOffset_sec = 18000;
const int   daylightOffset_sec = 0;

void checkandupdatvairables();
AsyncWebServer server(80);
AsyncWebServer OTAserver(81);

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/html; charset=UTF-8", "Not found");
}
///////////////////////////////////////////////////////////////////////

String THERMhttpGETRequest(const char* serverName) {
  WiFiClient client;
  HTTPClient http;
    
  // Your Domain name with URL path or IP address with path
  http.begin(client, serverName);
  
  // Send HTTP POST request
  int httpResponseCode = http.GET();
  
  String payload = "--"; 
  
  if (httpResponseCode>0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    if(httpResponseCode == 200){
      data_ok = 1;
    }
    else{
      data_ok = 0;
    }
    payload = http.getString();
  }
  else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  // Free resources
  http.end();

  return payload;
}

///////////////////////////////////////////////////////////////////////


String httpGETRequest(const char* commandforserver) {
  HTTPClient http;
  String payload = "--";
  bool httpInitResult =  http.begin(commandforserver);
  Serial.println(commandforserver);
  if ( httpInitResult == false ) {
    Serial.println("http.begin() failed!"); //debug
    retry_count -= 1;
    if (retry_count < 1) {
      retry_count = 10;
      Serial.println("begin failed...");
    }
  }
  else {
    int httpCode = http.GET();
    if (httpCode > 0) { //Check for the returning code
      payload = http.getString();
      Serial.println(payload);

      DynamicJsonDocument doc(1024);
      deserializeJson(doc, payload);

      String device_id = doc["device_id"];
      String lstcmd    = doc["lastcmd"];
      long packet_id = doc["packet_id"];
      if (packet_id != NULL) {
          packet_sequence = packet_id;
      }
      //-------TRY MATCHING DEVICE ID, THEN CHK IF MESSAGE CONTAINS COMMAND THAT IS SENT--------//
      //-------FROM APP TO SERVER WHILE APP WAS IN SERVER MODE--------//
      //-------ASSIGN THAT COMMAND TO LASTCMD VARIABLE SO THAT HARDWARE CAN READ IT--------//
      if (strstr(myID.c_str(), device_id.c_str())) {
        if (lstcmd != "" && lstcmd != "null") {
          lastcmd = lstcmd;
          if (strstr(lastcmd.c_str(), "startble")) {
            Serial.println("server cmd, ble start");
            delay(1000);
            switchblecounter = 30;
            forceBLE = 1;
            disconnect_wifi = 1;
            //ESP.restart();
          }
          else {
            checkandupdatvairables();
            commandrcvd = 1;
            String combineddata = "http://209.38.236.253/api/clearlastcmd?id=" + myID;
            Serial.println(combineddata);
            payload = "--";
            httpInitResult =  http.begin(combineddata);
            if (httpInitResult == true) {
              int httpCode = http.GET();
              if (httpCode > 0) {
                payload = http.getString();
                Serial.println(payload);
              }
            }
          }
        }
      }
    }
    else {
      counter++;
      Serial.print("Counter =");Serial.print(counter);
      Serial.println("Error on HTTP request");
    }
  }
  http.end(); //Free the resources
  return payload;
}

void TIME_SPLIT(){
    for(int u = 0; u<=31; u++){Time_data[u] = 0;}
    pos = SchString.indexOf('=');pos1= SchString.indexOf('=', pos +1);pos2= SchString.indexOf('=', pos1 +1);
    Pos = SchString.indexOf('|');Pos1= SchString.indexOf('|', Pos +1);
    TimeString[0] = SchString.substring(pos+1,Pos);TimeString[1] = SchString.substring(pos1+1,Pos1);
    TimeString[2] = SchString.substring(pos2 + 1);
    
    for(int p = 0; p <= 2; p++){
    pos = 0;
    for(int i = 0; i < TimeString[p].length(); i++){
      comma_index[pos] = TimeString[p].indexOf(',',i);
      Serial.print(pos);Serial.print(":");Serial.print(comma_index[pos]);Serial.print("  ");
      i = comma_index[pos];
      pos++;
    }
    Serial.println();Serial.println(pos);
    STR_buffer = TimeString[p].substring(0,comma_index[0]);if( p < 2 ){Time_data[STR_buffer.toInt() + (p*24)] = 1;}
    Serial.print(STR_buffer);Serial.print(" ");
    if( pos > 1){
    for(int u = 0; u <= pos-3; u++ ){
      STR_buffer = TimeString[p].substring(comma_index[u]+1,comma_index[u+1]);Time_data[STR_buffer.toInt() + (p*24)] = 1;
      Serial.print(STR_buffer);Serial.print(" ");
    }
    STR_buffer = TimeString[p].substring(comma_index[pos-2]+1,TimeString[p].length());Time_data[STR_buffer.toInt() + (p*24)] = 1;
    Serial.print(STR_buffer);Serial.print(" ");
    }
   }
   Serial.println();
  
   for(int u = 0; u<=31; u++){ EEPROM.write(u,Time_data[u]);Serial.print(" ");Serial.print(Time_data[u]);}EEPROM.commit();
   SCH_update = 1;
   Serial.println(); // from application// 0 17 04 22 03 08 50 0
} 
String splitvalue() {

  if (strstr(lastcmd.c_str(), "hoursch")) {
    return lastcmd;
  }
  int i1 = lastcmd.indexOf('=');
  int i2 = lastcmd.indexOf('=', i1 + 1);
  //Serial.println(i1);Serial.println(i2);
  String keyvar = lastcmd.substring(0, i1);
  String rcvdvalue = lastcmd.substring(i1 + 1, i2);
  return rcvdvalue;
}
void checkandupdatvairables() {

  int i1 = lastcmd.indexOf('=');
  int i2 = lastcmd.indexOf('=', i1 + 1);
  String cmdcmp = lastcmd.substring(0, i1);

  if (cmdcmp == "dmptemp") {
    dmptemp = splitvalue();
  }
  else if (cmdcmp == "dmptempsp") {
    dmptempsp = splitvalue();
    SP = dmptempsp.toInt();
    if(prevSP != SP){
      // exflg = 1;
      Serial.print("EXFLG = ");Serial.println(exflg);
      prevSP = SP;
      EEPROM.write(33,SP);EEPROM.commit();
    }
  }
  else if (cmdcmp == "dampertsw") {
    dampertsw = splitvalue().toInt();
    // if(dampertsw != damperswitch){
      
    // }
    if(prevdampertsw != dampertsw){
      damperswitch = dampertsw;
      cloudflag = 1;
      // exflg = 1;
      Serial.print("EXFLG = ");Serial.println(exflg);
      prevdampertsw = dampertsw;
      EEPROM.write(34,dampertsw);EEPROM.commit();
    }
  }
  else if (cmdcmp == "supcfm") {
    supcfm = splitvalue();
    supcfm_update = 1;
  }
  else if (cmdcmp == "retcfm") {
    retcfm = splitvalue();
    retcfm_update = 1;
  }
  else if (cmdcmp == "dampstate") {
    dampstate = splitvalue().toInt();
  }
  else if (cmdcmp == "timenow") {
    timenow = splitvalue();
  }
  else if (cmdcmp == "hoursch") {
    timesch = splitvalue();
    SchString = timesch;
    TIME_SPLIT();
  }
  else if (cmdcmp == "timeschen") {
    timeschen = splitvalue().toInt();
    if(prevtimeschen != timeschen){
      // exflg = 1;
      Serial.print("EXFLG = ");Serial.println(exflg);
      prevtimeschen = timeschen;
      EEPROM.write(36,timeschen);EEPROM.commit();
    }
  }
  else if (cmdcmp == "alarm_string") {
    alarm_string = splitvalue();
  }
  else if (cmdcmp == "seasonsw") {
    seasonsw = splitvalue().toInt();
    if(prevseasonsw != seasonsw){
      // exflg = 1;
      Serial.print("EXFLG = ");Serial.println(exflg);
      prevseasonsw = seasonsw;
      EEPROM.write(35,seasonsw);EEPROM.commit();
    }
  }
  Serial.println(lastcmd);
}
void getcommand(AsyncWebServerRequest *request) {
  int params = request->params();
  AsyncWebParameter* px = request->getParam(0);
  if (px->isFile()) { //p->isPost() is also true
    Serial.printf("FILE[%s]: %s, size: %u\n", px->name().c_str(), px->value().c_str(), px->size());
  } else if (px->isPost()) {
    Serial.printf("POST[%s]: %s\n", px->name().c_str(), px->value().c_str());
  } else {
    Serial.printf("GET[%s]: %s\n", px->name().c_str(), px->value().c_str());
    lastcmd = px->name() + "=" + px->value();
    Serial.print("CMD FROM LOCAL=");
    Serial.println(lastcmd);
    checkandupdatvairables();

    DynamicJsonDocument doc(1024);
    deserializeJson(doc, dataarray);

    doc["msg"] = "Data Updated Successfully";
    dataarray = "";
    serializeJson(doc, dataarray);
    request->send(200, "text/html; charset=UTF-8", dataarray);
  }
}
//-------THIS COMPLETE BLOCK IS FIRED WHEN DEVICE IS ACCESSED VIA LOCAL IP----//
void server_setup() {
  server.on("/getinfo", HTTP_GET, [] (AsyncWebServerRequest * request) {
    String message;
    Serial.println("getinfo");
    request->send(200, "text/html; charset=UTF-8", deviceinfo);
  });
  String chkID = "/" + myID;
  server.on(chkID.c_str(), HTTP_GET, [] (AsyncWebServerRequest * request) {
    String message;
    message = request->url();
    //-------SEND DATA STRING TO APP DIRECTLY FROM LOCAL IP--------//
    if (request->hasParam("getdata")) {

      int params = request->params();
      AsyncWebParameter* px = request->getParam(1);
      Serial.printf("GET[%s]: %s\n", px->name().c_str(), px->value().c_str());

      String packetvalue =  px->value();
      packet_sequence = packetvalue.toInt();
      makestring();
      request->send(200, "text/html; charset=UTF-8", dataarray);

    }
    //-------SEND DASHBOARD DATA STRING TO APP DIRECTLY FROM LOCAL IP--------//
    else if (request->hasParam("getdata_dashboard")) {
      int params = request->params();
      AsyncWebParameter* px = request->getParam(1);
      Serial.printf("GET[%s]: %s\n", px->name().c_str(), px->value().c_str());

      String packetvalue =  px->value();
      packet_sequence = packetvalue.toInt();
      makestring();
      request->send(200, "text/html; charset=UTF-8", dataarraydashboard);
    }
    //-------WHEN YOU GET STARTBLE FROM LOCAL IP, START BLE--------//
    else if (request->hasParam("startble")) {
      request->send(200, "text/html; charset=UTF-8", "BLE started successfully.");
      switchblecounter = 0;
      disconnect_wifi = 1;
    }
    //-------IF NOTHING ABOVE MATCHES, THEN ITS COMMAND FROM APP, TRY MATCHING THAT--------//
    else {
      getcommand(request); //see if global update is ok or it needs local update
      Serial.println("\r\ncommand received false");
    }
  });
  server.on("/post", HTTP_POST, [](AsyncWebServerRequest * request) {
    String message;
    request->send(200, "text/html; charset=UTF-8", "Hello, POST: " + message);
  });
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  AsyncElegantOTA.begin(&OTAserver);
  server.onNotFound(notFound);
  OTAserver.onNotFound(notFound);
  server.begin();
  OTAserver.begin();
}
