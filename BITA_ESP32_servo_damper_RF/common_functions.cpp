#include "common_functions.h"
#include "EEPROM.h"
#include "externvars.h"
#include <ArduinoJson.h>
String getValue(String data, char separator, int index){
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length()-1;

  for(int i=0; i<=maxIndex && found <=index; i++){
    if(data.charAt(i)==separator || i==maxIndex){
      found++;
      strIndex[0] = strIndex[1]+1;
      strIndex[1] = (i==maxIndex) ? i+1 : i;
    }
  }
  return found>index ? data.substring(strIndex[0], strIndex[1]) : "";
}
String read_String(int add){
  char data[500];
  int len = 0;
  unsigned char k;
  k = EEPROM.read(add);
  while(k != '\0' && len< 500){
    k = EEPROM.read(add+len);
    data[len] = k;
    len++;
  }
  data[len] = '\0';
  return String(data);
}

void makestring(){
    dataarray="";
    dataarraydashboard="";
    
   if(device_type==3){
    DynamicJsonDocument jsonBuffer(1024);
    //jsonBuffer["device_id"] = myID;
    jsonBuffer["seasonsw"] = seasonsw;
    jsonBuffer["dmptemp"] = dmptemp;
    jsonBuffer["dmptempsp"] = dmptempsp;
    jsonBuffer["dampertsw"] = dampertsw;
    jsonBuffer["supcfm"]=supcfm; 
    jsonBuffer["retcfm"]=retcfm; 
    jsonBuffer["timesch"]=timesch; 
    jsonBuffer["timeschen"] = timeschen; 
    jsonBuffer["dampstate"] = dampstate;
    jsonBuffer["alarm"] = alarm_string;
    jsonBuffer["timenow"] = timenow;
    jsonBuffer["ip_address"] = myIP;
    jsonBuffer["packet_id"] = packet_sequence;
    serializeJson(jsonBuffer, dataarray);

    DynamicJsonDocument jsonBufferdashboard(1024);
    jsonBufferdashboard["device_id"] = myID;
    jsonBufferdashboard["dmptemp"] = dmptemp;
    jsonBufferdashboard["dmptempsp"] = dmptempsp;
    jsonBufferdashboard["dampstate"] = dampstate;
    jsonBufferdashboard["alarm"] = alarm_string;
    jsonBufferdashboard["timenow"] = timenow;
    jsonBufferdashboard["ip_address"] = myIP;
    jsonBufferdashboard["packet_id"] = packet_sequence;
    serializeJson(jsonBufferdashboard, dataarraydashboard);
  }
   
}
