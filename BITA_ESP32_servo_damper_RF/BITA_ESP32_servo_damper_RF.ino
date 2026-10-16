#include "variables.h"
#include "common_functions.h"
#include "blethread.h"
#include "wifithread.h"
#include <ESP32Servo.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include <OneWire.h>
#include <Wire.h>
#include <DallasTemperature.h>

long data_send = 0;
int chng_flg = 1;
int previr_state;
int offlineledtm;

bool RF_FLAG = false;
bool configflag = true;


String dampertemp1;
String dampersendswitch1;
String damperseason1;
String dampersetpoint1;
String dampersch1;
String Ping_chk;

int bluetoothvar = 0;

#define ONE_WIRE_BUS 19
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

Servo SUPPLY_MOTOR;
Servo BYPASS_MOTOR;
OneWire  ds(19);
unsigned long IR_time;

#define SUPPLY 2
// #define BYPASS 13
#define IND_PIN 25
#define potPin 34

#define EEPROM_SIZE 200


extern int wifiAddr;


void loopFunction(){
    
    byte ch;
  if (Serial.available() > 0) {
    
    ch = Serial.read();
    sdata += (char)ch;
    if (ch=='\r') {
       sdata.trim();
       process_cmd();
       sdata = "";
    }
  }
  if(datainterrupt){
    //SCH_ROUTINE();
    if(commandrcvd==1){
      commandrcvd=0;
      makestring();
      Serial.println("\r\nSENDING TO HARDWARE\r\n");
      Serial.println(dataarray);
    }
    esp_task_wdt_reset();
    datainterrupt = false;
    //when we want to turn on BLE, we disconnect WIFI, and then leave program on its own.
    if(disconnect_wifi==1){
        EEPROM.write(52, disconnect_wifi);EEPROM.commit();
        disconnect_wifi=0;
        delay(1000);
        WiFi.disconnect();
        delay(1000);
        Serial.println("WIFI Disconnected...");
        ESP.restart();
      }
    //program sees wifi is dc and ble is also not connected to any phone, it will turn on BLE
    if(WiFi.status()!= WL_CONNECTED && deviceConnected==false){ 
      wifi_status=0;
      // if (damperswitch = 2){
      // OPEN();Serial.println("close_IR-OPEN");    
      // } else {
      // CLOSE();Serial.println("closing");  
      // } 
      // ========================================================================== open damper if wifi not connected.
      if(switchblecounter>1){
        if(forceBLE==1){forceBLE=0;modeIdx=1;Serial.println("switching BLE on.");bleTask();}
        switchblecounter-=1;
        //This line is just a message for testing device switching and can be turned off.
        Serial.print("Waiting for ");
        Serial.print(switchblecounter);
        Serial.print(" seconds before switching device to ");
        if(modeIdx==0){Serial.println("BLE");connectToWiFi();}
        else{Serial.println("BLE");}
      }else{
        switchblecounter=10; //Switch counter value 
        if(modeIdx==0){modeIdx=1;Serial.println("switching BLE on.");bleTask();}
        else{
          // Serial.println("switching WIFI on.");ESP.restart();
          blestop();
          configflag = false;
        }
        //modeIdx=0;connectToWiFi();
      }
    }else{
      if(WiFi.status()== WL_CONNECTED && deviceConnected==false){
        configflag = true;
        wifi_status=1;
        String requestresponse="";
        NTP_TIME();
        makestring();
        String combineddata = savedatacommand + dataarray;
        requestresponse= httpGETRequest(combineddata.c_str());  
        
        if(tmp_req){ tmp_req = 0;time3 = millis();
        temper = sensors.getTempCByIndex(0);
        sensors.setWaitForConversion(false);sensors.requestTemperatures();sensors.setWaitForConversion(true);
        Serial.println(temper);
        if(temper > 0){dmptemp = String(temper);} else {dmptemp = "0";}
        //dmptemp = String(temper);
        }
        if (temper != tprev) {
          op = 1;Serial.println("temper != tprev");
          tprev = temper;
          time1 = millis();
          //exflg = 1;
          }
      }
      else if(deviceConnected==true){
        wifi_status=0;
        if(disconnection_counter>0){disconnection_counter -=1;}else{Serial.println("restarting to turn off BLE...");delay(500);ESP.restart();} //connect bluetooth for 300 seconds in total to avoid idle stuck.
        Serial.print("device is in BLE mode and connected to phone. it will turn off in ");
        Serial.print(disconnection_counter);
        Serial.println(" seconds.");
      }
    }
  }
}

bool SCH_ROUTINE(){// to application
//  
//  SchScan[7] = myRtc.getPeriod();if(SchScan[7] == 1){formate = 12;}else{formate = 0;}deviceTime = "";
//  SchScan[1] = myRtc.getDate();SchScan[2] = myRtc.getMonth();SchScan[3] = myRtc.getYear();SchScan[0] = myRtc.getDay();
//  SchScan[4] = myRtc.getHour() + formate;SchScan[5] = myRtc.getMinute();SchScan[6] = myRtc.getSecond();
//  deviceTime = SchScan[0];
   deviceTime = "";
  for(int y = 1;y <= 6 ; y++){if(SchScan[y]<10){deviceTime = deviceTime + "0";deviceTime = deviceTime + String(SchScan[y]);}
  else {deviceTime = deviceTime + String(SchScan[y]);}Serial.print(SchScan[y]);Serial.print(" ");}
  timenow = deviceTime;
  count = 0;
  timesch="hoursch=";for (int i = 0; i < 23; i++){if(Time_data[i] == 1){STR_buffer = STR_buffer + String(i);STR_buffer = STR_buffer + ",";}}
  for(int u = 0; u<=23; u++){ count = Time_data[u] + count;}Serial.print("count of saved hours ");Serial.println(count);if(count < 0 | count > 24){STR_buffer = "24,";}
  timesch = timesch + STR_buffer.substring(0,STR_buffer.length() - 1);timesch = timesch + "|daysch=";STR_buffer = "";
  for (int i = 24; i < 31; i++){if(Time_data[i] == 1){STR_buffer = STR_buffer + String(i-24);STR_buffer = STR_buffer + ",";}}count = 0;
  for(int u = 24; u<=31; u++){  count = Time_data[u] + count;}Serial.print("count of saved days ");Serial.println(count);if(count < 0 | count > 6){STR_buffer = "7,";}
  timesch = timesch + STR_buffer.substring(0,STR_buffer.length() - 1);timesch = timesch + "|datetime_now=";STR_buffer = "";
  timesch = timesch + deviceTime;Serial.println(timesch);

  if(Time_data[SchScan[4]] && Time_data[for_schrout[SchScan[0]] + 23]){//Serial.print("hour : ");Serial.println(Time_data[SchScan[4]]);
  //Serial.print("day : ");Serial.println(Time_data[SchScan[0]+24]);Serial.print("enable : ");Serial.println(timeschen);
  Serial.println("true");
  return true;} 
  else{  
  //Serial.print("hour : ");Serial.println(Time_data[SchScan[4]]);Serial.print("day : ");Serial.println(Time_data[SchScan[0]+24]);
 // Serial.print("enable : ");Serial.println(timeschen);
  Serial.println("false"); 
  return false;}
}

void NTP_TIME(){
struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");Serial.print("   ");Serial.print(timeinfo.tm_hour);Serial.print(" ");
  deviceTime = "";
  Serial.print("day=" );Serial.println(timeinfo.tm_wday);
  tmp=day_app[timeinfo.tm_wday];
  SchScan[0] = tmp;SchScan[1] = timeinfo.tm_mday; SchScan[2] = timeinfo.tm_mon + 1; SchScan[3] = timeinfo.tm_year - 100;
  SchScan[4] = timeinfo.tm_hour; SchScan[5] = timeinfo.tm_min; SchScan[6] = timeinfo.tm_sec;
}
void DEVICE_INIT(){
  count = 0;
  if (!EEPROM.begin(EEPROM_SIZE)){Serial.println("failed to initialise EEPROM"); delay(1000);}timesch="hoursch=";
  for (int i = 0; i < 31; i++){Time_data[i] = EEPROM.read(i);Serial.print(Time_data[i]);} STR_buffer = "";
  Serial.print(" ");pos = timesch.indexOf('=');pos1= timesch.indexOf('=', pos +1);pos2= timesch.indexOf('=', pos1 +1);
  for (int i = 0; i < 23; i++){if(Time_data[i] == 1){STR_buffer = STR_buffer + String(i);STR_buffer = STR_buffer + ",";}}
  for(int u = 0; u<=23; u++){ count = Time_data[u] + count;}Serial.print("count of saved hours ");Serial.println(count);if(count == 0){STR_buffer = "24,";}
  timesch = timesch + STR_buffer.substring(0,STR_buffer.length() - 1);timesch = timesch + "|daysch=";STR_buffer = "";
  for (int i = 24; i < 31; i++){if(Time_data[i] == 1){STR_buffer = STR_buffer + String(i-24);STR_buffer = STR_buffer + ",";}}count = 0;
  for(int u = 24; u<=31; u++){  count = Time_data[u] + count;}Serial.print("count of saved days ");Serial.println(count);if(count == 0){STR_buffer = "7,";}
  timesch = timesch + STR_buffer.substring(0,STR_buffer.length() - 1);timesch = timesch + "|datetime_now=";STR_buffer = "";
  SCH_ROUTINE();timesch = timesch + deviceTime;Serial.println(timesch);scfm[0] = EEPROM.read(40);scfm[1] = EEPROM.read(41);
  SP = EEPROM.read(33);dmptempsp = String(SP);seasonsw = EEPROM.read(35);
  // damperswitch = EEPROM.read(34);dampertsw = damperswitch;
  timeschen =  EEPROM.read(36);
  rcfm[0] = EEPROM.read(50);rcfm[1] = EEPROM.read(51);bluetoothvar = EEPROM.read(52);
  // ir_state = EEPROM.read(53); previr_state = ir_state;
  minpot = (EEPROM.read(54) << 8) + EEPROM.read(55);
  maxpot = (EEPROM.read(56) << 8) + EEPROM.read(57);
  supcfm = "";supcfm = String(scfm[0]) + "-";supcfm = supcfm + String(scfm[1]);
  // retcfm = "";retcfm = String(rcfm[0]) + "-";retcfm = retcfm + String(rcfm[1]);
  Serial.print("supcfm:");Serial.println(supcfm);
  Serial.print("retcfm:");Serial.println(retcfm);
  // Serial.print("timeschen:");Serial.println(timeschen);
}
void SCFM(){
  if(supcfm_update){

  scfm[2] = scfm[0];scfm[3] = scfm[1];pos = supcfm.indexOf('-');
  scfm[0] = supcfm.substring(0,pos).toInt();scfm[1] = supcfm.substring(pos+1,supcfm.length()).toInt();
  EEPROM.write(40,scfm[0]);EEPROM.write(41,scfm[1]);EEPROM.commit();
  
  if (dampstate == 0){ //damper closed
  if(scfm[0] > scfm[2]) { for(int deg = scfm[2]*1.8;deg < scfm[0]*1.8; deg++){ //damper closed,lower limit increased
     SUPPLY_MOTOR.write(deg);delay(15);}}
  if(scfm[0] < scfm[2]) { for(int deg = scfm[2]*1.8;deg > scfm[0]*1.8; deg--){ //damper closed,lower limit decreased
     SUPPLY_MOTOR.write(deg);delay(15);}}
  }

  if (dampstate == 1){ //damper open
  if(scfm[1] > scfm[3]) { for(int deg = scfm[3]*1.8;deg < scfm[1]*1.8; deg++){ //damper closed,lower limit increased
     SUPPLY_MOTOR.write(deg);delay(15);Serial.println(deg);}}
  if(scfm[1] < scfm[3]) { for(int deg = scfm[3]*1.8;deg > scfm[1]*1.8; deg--){ //damper closed,lower limit decreased
     SUPPLY_MOTOR.write(deg);delay(15);Serial.println(deg);}}
  }
  supcfm_update = 0;}
}//scfm[1] = deg;

void SCFM_INIT(){

  // if(WiFi.status()== WL_CONNECTED){
    Serial.print(scfm[0]);Serial.print(" ");Serial.print(scfm[1]);Serial.println();
    supcfm = String(scfm[0]);supcfm = supcfm + "-";supcfm = supcfm + String(scfm[1]);
    if(analogRead(potPin) - minpot > 0){
        // scfm[3] = analogRead(potPin);
        // Serial.print("scfm[3]: ");Serial.println(scfm[3]);
        // spos = (analogRead(potPin) - minpot) / 9.5;
        scfm[3] = map(analogRead(potPin), minpot, maxpot, 0, 100);
        // scfm[3] = (analogRead(34) - 930)/13;
        Serial.print("analog: ");Serial.println(scfm[3]);
        Serial.print("scfm[1]: ");Serial.println(scfm[1]);
        for(int deg = scfm[3]*1.8;deg < 180; deg++){
        SUPPLY_MOTOR.write(deg);delay(15);}
        scfm[3] = map(analogRead(potPin), minpot, maxpot, 0, 100);
        Serial.print("analog: ");Serial.println(scfm[3]);
        Serial.print("scfm[1]: ");Serial.println(scfm[1]);
        for(int deg = scfm[3]*1.8;deg > 0; deg--){
        SUPPLY_MOTOR.write(deg);delay(15);}
        scfm[3] = map(analogRead(potPin), minpot, maxpot, 0, 100);
        Serial.print("analog: ");Serial.println(scfm[3]);
        Serial.print("scfm[1]: ");Serial.println(scfm[1]);
      }
      else{
        scfm[3] = 0;
        for(int deg = scfm[3]*1.8;deg < 180; deg++){
        SUPPLY_MOTOR.write(deg);delay(15);}
        scfm[3] = map(analogRead(potPin), minpot, maxpot, 0, 100);
        Serial.print("analog: ");Serial.println(scfm[3]);
        Serial.print("scfm[1]: ");Serial.println(scfm[1]);
        scfm[3] = map(analogRead(potPin), minpot, maxpot, 0, 100);
        // for(int deg = scfm[3]*1.8;deg > 0; deg--){
        // SUPPLY_MOTOR.write(deg);delay(15);}
        // scfm[3] = map(analogRead(potPin), minpot, maxpot, 0, 100);
        // Serial.print("analog: ");Serial.println(scfm[3]);
        // Serial.print("scfm[1]: ");Serial.println(scfm[1]);
      }



      // if(scfm[1] > scfm[3]) { for(int deg = scfm[3]*1.8;deg < scfm[1]*1.8; deg++){ //damper closed,lower limit increased
      //   SUPPLY_MOTOR.write(deg);delay(15);}}
      // if(scfm[1] < scfm[3]) { for(int deg = scfm[3]*1.8;deg > scfm[1]*1.8; deg--){ //damper closed,lower limit decreased
      //   SUPPLY_MOTOR.write(deg);delay(15);}}
      // dampstate = 0;
  // }

  // Serial.print("ir_state = ");Serial.println(ir_state);

    
  //   if(digitalRead(27) == LOW){
  //    //Serial.print("time passed when ir changed: ");Serial.println(millis() - IR_time);
  //   //  digitalWrite(IND_PIN,HIGH);
  //    if((cloudflag != 1 || ir_state != 2) && previr_state != 2){
  //     Serial.print("///////////////////////IR_STATE 1/////////////////");
  //     // digitalWrite(IND_PIN,HIGH);
  //     // delay(500);
  //     // digitalWrite(IND_PIN,LOW);
  //     dampertsw = 2;
  //     damperswitch = 2;
  //     prevdampertsw = dampertsw;
  //     ir_state = 2;
  //     previr_state = ir_state;
  //     cloudflag = 0;
  //     //exflg = 1;
  //     // EEPROM.write(53,ir_state);EEPROM.commit();
  //     // if(WiFi.status() != WL_CONNECTED){
  //       OPEN();
  //     // }
  //    }
  //   //  return true; 
  //   }
  //   if(digitalRead(27) == HIGH) {
  //   //  digitalWrite(IND_PIN,LOW);
  //    if((cloudflag != 1 || ir_state != 0) && previr_state != 0){
  //     Serial.print("///////////////////////IR_STATE 2/////////////////");
  //     // digitalWrite(IND_PIN,HIGH);
  //     // delay(500);
  //     // digitalWrite(IND_PIN,LOW);
  //     dampertsw = 0;
  //     damperswitch = 0;
  //     prevdampertsw = dampertsw;
  //     ir_state = 0;
  //     previr_state = ir_state;
  //     cloudflag = 0;
  //     //exflg = 1;
  //     // EEPROM.write(53,ir_state);EEPROM.commit();
  //     // if(WiFi.status() != WL_CONNECTED){
  //       CLOSE();
  //     // }
  //    }
  //   //  return false;
  //   }
    // delay(1000);
    // dampertsw = 0;
    // damperswitch = 0;
    // prevdampertsw = dampertsw;
    // CLOSE();
  
  }

void MANUAL_OP() {
  Serial.print("op:");Serial.println(op);
  if(timeschen == 0 || RF_FLAG == true){SHED = true;} else {SHED = SCH_ROUTINE();}
   if(RF_FLAG == true){
      digitalWrite(IND_PIN,HIGH);
      delay(250);
      digitalWrite(IND_PIN,LOW);
      RF_FLAG = false;
      timeschen = 0;
      prevtimeschen = timeschen;
      EEPROM.write(36,timeschen);EEPROM.commit();

    }
  
  if(SHED){
  if ( damperswitch == 2) {
    OPEN();Serial.println("opening");
  } else if ( damperswitch == 0 ) {
    // if (IR_INPUT()){
    // OPEN();Serial.println("close_IR-OPEN");    
    // } else {
    CLOSE();Serial.println("closing");  
    // }
  } else if( damperswitch == 1 ){
    AUTO_OP();
  }} else {
    CLOSE();Serial.println("shed false closing");
  }
 
}
void AUTO_OP() {
  Serial.println("Auto mode");
  if (seasonsw == 0){
    Serial.print("seasonsw:");Serial.print(seasonsw);
  if ( temper > SP && op == 0 ) {
    OPEN();
    Serial.println("openning");
  } else if ( temper <= SP && op == 0 ) {
    CLOSE();
    Serial.println("closing");
  }} else if (seasonsw == 1){
    Serial.print("seasonsw:");Serial.print(seasonsw);
  if ( temper < SP && op == 0) {
    OPEN();
    Serial.println("openning");
  } else if ( temper >= SP && op == 0) {
    CLOSE();
    Serial.println("closing");
  }}
}
void OPEN(){
   if(LastState == 2){
   spos = analogRead(potPin);
    // spos = (analogRead(potPin) - minpot) / 9.5;
    spos = map(spos, minpot, maxpot, 0, 180);
   if( spos < 0 ){ spos = 0;} else if(spos > scfm[1]*MaxDegree){spos = scfm[1]*MaxDegree;}
   if( spos >= (scfm[0]*MaxDegree - 20) && spos <= (scfm[0]*MaxDegree + 20)){
   for(int deg = scfm[0]*MaxDegree;deg < scfm[1]*MaxDegree; deg++){ 
    // Serial.println(deg);
    SUPPLY_MOTOR.write(deg);delay(15);}
   } else { 
   for(int deg = spos; deg < scfm[1]*MaxDegree; deg++){ 
    Serial.println(deg);
    SUPPLY_MOTOR.write(deg);delay(15);}} 
   
  //  BYPASS_STATUS = analogRead(34);
  //  if (BYPASS_STATUS < 4000){
  //  for(int deg = rcfm[1]*MaxDegree; BYPASS_STATUS < 4050; deg--){ 
  //  BYPASS_MOTOR.write(deg);delay(15);BYPASS_POS = deg;BYPASS_STATUS = analogRead(34);if (BYPASS_POS < 0){BYPASS_POS = 0;break;}}}
  //  BYPASS_POS--;
   dampstate = 1;LastState = 1;} 
   
   else if(LastState == 0){ 
   spos = analogRead(potPin);
    // spos = (analogRead(potPin) - minpot) / 9.5;
    spos = map(spos, minpot, maxpot, 0, 180);; 
   if( spos < 0 ){ spos = 0;} else if(spos > scfm[1]*MaxDegree){spos = scfm[1]*MaxDegree;}
   if( spos >= (scfm[0]*MaxDegree - 20) && spos <= (scfm[0]*MaxDegree + 20)){
   for(int deg = scfm[0]*MaxDegree;deg < scfm[1]*MaxDegree; deg++){ 
    // Serial.println(deg);
    SUPPLY_MOTOR.write(deg);delay(15);}
   } else { 
   for(int deg = spos; deg < scfm[1]*MaxDegree; deg++){ 
    // Serial.println(deg);
    SUPPLY_MOTOR.write(deg);delay(15);}} 
   
  //  BYPASS_STATUS = analogRead(34);
  //  if (BYPASS_STATUS < 4000){
  //  for(int deg = rcfm[1]*MaxDegree; BYPASS_STATUS < 4050; deg--){ 
  //  BYPASS_MOTOR.write(deg);delay(15);BYPASS_POS = deg;BYPASS_STATUS = analogRead(34);if (BYPASS_POS < 0){BYPASS_POS = 0;break;}}}
  //  BYPASS_POS--;
   dampstate = 1;LastState = 1;}
   }
   
void CLOSE() {
  // Serial.println("HELLO CLOSE");
  if (LastState == 1) {
    // Serial.println("HELLO LS 1");
    spos = analogRead(potPin);
    // spos = (analogRead(potPin) - minpot) / 9.5;
    spos = map(spos, minpot, maxpot, 0, 180);
    // Serial.print("pot value: ");
    // Serial.print(spos);
    // Serial.println();
    if (spos < 0) {
      spos = 0;
    } else if (spos > scfm[1] * MaxDegree) {
      spos = scfm[1] * MaxDegree;
    }
    if (spos >= (scfm[1] * MaxDegree - 20) && spos <= (scfm[1] * MaxDegree + 20)) {
      for (int deg = scfm[1] * MaxDegree; deg > scfm[0] * MaxDegree; deg--) {
        SUPPLY_MOTOR.write(deg);
        delay(15);
      }
    } else {
      for (int deg = spos; deg > scfm[0] * MaxDegree; deg--) {
        SUPPLY_MOTOR.write(deg);
        delay(15);
      }
    }
    dampstate = 0;
    LastState = 2;
  } else if (LastState == 0) {
    // Serial.println("HELLO LS 0");
    spos = analogRead(potPin);
    // spos = (analogRead(potPin) - minpot) / 9.5;
    spos = map(spos, minpot, maxpot, 0, 180);
    // Serial.print("pot value: ");
    // Serial.print(spos);
    // Serial.println();
    if (spos < 0) {
      spos = 0;
    } else if (spos > scfm[1] * MaxDegree) {
      spos = scfm[1] * MaxDegree;
    }
    if (spos >= (scfm[1] * MaxDegree - 20) && spos <= (scfm[1] * MaxDegree + 20)) {
      for (int deg = scfm[1] * MaxDegree; deg > scfm[0] * MaxDegree; deg--) {
        SUPPLY_MOTOR.write(deg);
        delay(15);
      }
    } else {
      for (int deg = spos; deg > scfm[0] * MaxDegree; deg--) {
        SUPPLY_MOTOR.write(deg);
        delay(15);
      }
    }
    dampstate = 0;
    LastState = 2;
  }
}

  
void IRAM_ATTR IR_INPUT(){

  // Serial.print("ir_state = ");Serial.println(ir_state);
  
  // if (digitalRead(27) == HIGH){
  //   IR_time = millis();
  // }

    
    // if(1000 < (millis() - IR_time)){
    if(digitalRead(27) == HIGH){

     //Serial.print("time passed when ir changed: ");Serial.println(millis() - IR_time);
    //  digitalWrite(IND_PIN,HIGH);
     if((cloudflag != 1 || ir_state != 2) && previr_state != 2){
      // Serial.print("///////////////////////IR_STATE 1/////////////////");
      // digitalWrite(IND_PIN,HIGH);
      // delay(50);
      // digitalWrite(IND_PIN,LOW);
      dampertsw = 2;
      damperswitch = 2;
      prevdampertsw = dampertsw;
      ir_state = 2;
      previr_state = ir_state;
      cloudflag = 0;
      //exflg = 1;
      RF_FLAG = true;
      // EEPROM.write(53,ir_state);EEPROM.commit();
      // if(WiFi.status() != WL_CONNECTED){
      // OPEN();
      // }
     }
    //  return true; 
    } 
    if(digitalRead(27) == LOW) {
    //  digitalWrite(IND_PIN,LOW);
     if((cloudflag != 1 || ir_state != 0) && previr_state != 0){
      // Serial.print("///////////////////////IR_STATE 2/////////////////");
      // digitalWrite(IND_PIN,HIGH);
      // delay(50);
      // digitalWrite(IND_PIN,LOW);
      dampertsw = 0;
      damperswitch = 0;
      prevdampertsw = dampertsw;
      ir_state = 0;
      previr_state = ir_state;
      cloudflag = 0;
      //exflg = 1;
      RF_FLAG = true;
      // EEPROM.write(53,ir_state);EEPROM.commit();
      // if(WiFi.status() != WL_CONNECTED){
      // CLOSE();
      // }
     }
    //  return false;
    }
}


void IRAM_ATTR onTimer() {
   if(milscount>1501){
     milscount=0;
     datainterrupt = true; 
   }
   if(wifi_status==0 && deviceConnected==false && configflag==true){ 
    if(ledcounter>50){
      ledcounter=0;
      if(blink==0){digitalWrite(23,HIGH);blink =1;}else{digitalWrite(23,LOW); blink =0;} 
    }
   }
   if(wifi_status==0 && deviceConnected==false && configflag==false){
      if(blink==0){
        if(ledcounter>250){
          ledcounter=0;
          digitalWrite(23,LOW);blink = 1;
        }
      }
      else{
        if(ledcounter>5001){
          ledcounter=0;
          digitalWrite(23,HIGH); blink = 0;
        }
      }
   }
   else if(wifi_status==0 && deviceConnected==true){ 
    if(ledcounter>500){
      ledretrycount+=1;
      if(ledretrycount>5){
        ledcounter=0;
        ledretrycount=0;
      }else{
        ledcounter=400;
      }
      if(blink==0){digitalWrite(23,HIGH);blink =1;}else{digitalWrite(23,LOW); blink =0;} 
    }
   }
   else if(wifi_status==1 && deviceConnected==false){ 
    if(ledcounter>1001){
      ledcounter=0;
      if(blink==0){digitalWrite(23,HIGH);blink =1;}else{digitalWrite(23,LOW); blink =0;} 
    }
   }
   ledcounter++;
   milscount++;
}

void save_MAC(){
  // myData.MAC[0] = binaryToDecimal(macaddress.substring(0,2));
  Serial.print("MAC1 :");Serial.println(myData.MAC[0]);
  // myData.MAC[1] = binaryToDecimal(macaddress.substring(3,5));
  Serial.print("MAC2 :");Serial.println(myData.MAC[1]);
  // myData.MAC[2] = binaryToDecimal(macaddress.substring(6,8));
  Serial.print("MAC3 :");Serial.println(myData.MAC[2]);
  // myData.MAC[3] = binaryToDecimal(macaddress.substring(9,11));
  Serial.print("MAC4 :");Serial.println(myData.MAC[3]);
  // myData.MAC[4] = binaryToDecimal(macaddress.substring(12,14));
  Serial.print("MAC5 :");Serial.println(myData.MAC[4]);
  // myData.MAC[5] = binaryToDecimal(macaddress.substring(15,macaddress.length()));
  Serial.print("MAC6 :");Serial.println(myData.MAC[5]);
}

void setup() {
  Serial.begin(115200);
  pinMode(26,INPUT);
  pinMode(27, INPUT_PULLUP);
  Serial.println(xPortGetCoreID());
  Serial.print("\n\nbooting up device...\n\n");
  if(!EEPROM.begin(EEPROM_SIZE)){delay(1000);}
  //=========================================//
   sensors.begin();
   sensors.setWaitForConversion(false);
   sensors.requestTemperatures();
   sensors.setWaitForConversion(true);
   Wire.begin();
   ESP32PWM::allocateTimer(0);
   ESP32PWM::allocateTimer(1);
   ESP32PWM::allocateTimer(2);
   ESP32PWM::allocateTimer(3);
   SUPPLY_MOTOR.setPeriodHertz(50); 
  //  BYPASS_MOTOR.setPeriodHertz(50); 
   SUPPLY_MOTOR.attach(SUPPLY, 1000, 2000);
  //  BYPASS_MOTOR.attach(BYPASS, 1000, 2000);
  //  DEVICE_INIT();
  //  SCFM_INIT();
//   RCFM_INIT();
  
 
  pinMode (23, OUTPUT);
  pinMode (25, OUTPUT);

  DEVICE_INIT();

  if(minpot == 0 || maxpot == 0){
    for (int y = 0; y <= 180; y += 1) { 
		  SUPPLY_MOTOR.write(y);
		  delay(30);
      if(y == 0){
        delay(500);
        minpot = analogRead(34);
        Serial.print("Minpot_1 = ");Serial.println(minpot);
        // EEPROM.write(54,minpot);EEPROM.commit();
        EEPROM.write(54, minpot >> 8);
        EEPROM.write(55, minpot & 0xFF);
        EEPROM.commit();
      }
      if(y == 180){
        delay(500);
        maxpot = analogRead(34);
        Serial.print("Maxpot_1 = ");Serial.println(maxpot);
        EEPROM.write(56, maxpot >> 8);
        EEPROM.write(57, maxpot & 0xFF);
        EEPROM.commit();
      }
      Serial.print("y = ");Serial.println(y);
	  }
   
  }

  SCFM_INIT();

   //=========================================//
  macaddress = WiFi.macAddress();
  deviceinfo="";
  DynamicJsonDocument jsonBufferinfo(400);
  jsonBufferinfo["mac_address"] = macaddress;
  jsonBufferinfo["slave_mac_address"]=slave_mac_address;
  jsonBufferinfo["type"] = device_type;
  jsonBufferinfo["vers"] = devversion;
  jsonBufferinfo["id"]=myID; 
  jsonBufferinfo["disconnection_counter"]=disconnection_counter; 
  jsonBufferinfo["device_timeout_counter"]=switchblecounter; 
  jsonBufferinfo["wifi_channel"]=wifi_channel;
  jsonBufferinfo["wifissid"]=ssid;
  jsonBufferinfo["wifipassword"]=password;
  
  serializeJson(jsonBufferinfo, deviceinfo);
      
  savedatacommand = "http://209.38.236.253/api/savedata?id=" + myID  + "&data=";
  if(modeIdx==0){
    connectToWiFi();
  }
  else{
    bleTask();
  }

  attachInterrupt(digitalPinToInterrupt(27), IR_INPUT, CHANGE);

  timer = timerBegin(0, 80, true);                //Begin timer with 1 MHz frequency (80MHz/80)
  timerAttachInterrupt(timer, &onTimer, true);   //Attach the interrupt to Timer1
  unsigned int timerFactor = 1000000/SamplingRate; //Calculate the time interval between two readings, or more accurately, the number of cycles between two readings
  timerAlarmWrite(timer, timerFactor, true);      //Initialize the timer
  timerAlarmEnable(timer); 

  // esp_task_wdt_init(WDT_TIMEOUT, true); //enable panic so ESP32 restarts
  // esp_task_wdt_add(NULL); //add current thread to WDT watch
  data_send = millis();
}
//---PROCESS COMMANDS RCVD FROM SERIAL PORT-----//
void process_cmd(){
    Serial.println(sdata);
    if(sdata=="$BLE"){
      Serial.println("Switching to BLE Mode\n");
      bleTask();
      Serial.println("BLE Mode On\n");
    }
    else if(sdata=="$WIFI"){
      Serial.println("Switching to WIFI Mode");
    }
    else if(sdata=="$RST#"){
      Serial.println("Rebooting in 3 Seconds...");
      delay(1000);
      ESP.restart();
    }
}
//------------------END---------------------//
void connectToWiFi() {
    // digitalWrite(IND_PIN,HIGH);
    // delay(125);
    // digitalWrite(IND_PIN,LOW);
    //EEPROM.write(count_add,counter++);
    //EEPROM.commit();
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
    WiFi.setHostname(hostname.c_str());
    String receivedData;
    Serial.print("\nfetching wifi info...\n");
    receivedData = read_String(wifiAddr);
    if(receivedData.length() > 0){
      String wifiName = getValue(receivedData, ',', 0);
      String wifiPassword = getValue(receivedData, ',', 1);
      slave_mac_address = getValue(receivedData, ',', 2);
      ble_slave_mac_address = slave_mac_address;
      ble_slave_mac_address.replace("/",":");
      ccx2 = slave_mac_address.c_str();
          sscanf(ccx2, "%x/%x/%x/%x/%x/%x%*c",
                      &broadcastAddress[0], &broadcastAddress[1], &broadcastAddress[2],
                      &broadcastAddress[3], &broadcastAddress[4], &broadcastAddress[5]);
    if(bluetoothvar == 0){
      // if(wifiName.length() > 0 && wifiPassword.length() > 0 && slave_mac_address.length() > 0){
      //   Serial.print("wifi info success...\n");
      //   WiFi.disconnect(true);
      //   delay(200);
      //   ssid = wifiName.c_str();
      //   password = wifiPassword.c_str();
      //   WiFi.begin(wifiName.c_str(), wifiPassword.c_str());
      //   Serial.print("connecting to Wifi\n");

      //   if (WiFi.waitForConnectResult() != WL_CONNECTED) {
      //     Serial.printf("connection to WiFi failed!\n");
          
      //   }
      //   else{
      //     Serial.println();
      //     myIP = WiFi.localIP().toString();
      //     Serial.print("connected with IP: "); Serial.println(myIP);
      //     wifi_channel = WiFi.channel();
      //     server_setup();
      //   }
      // }

      if(wifiName.length() > 0 && wifiPassword.length() > 0){
        WiFi.disconnect(true);
        Serial.print("wifi info success...Configuring in STA\n");
        delay(200);
        ssid = wifiName.c_str();
        password = wifiPassword.c_str();
        WiFi.begin(wifiName.c_str(), wifiPassword.c_str());
        Serial.print("connecting to Wifi\n");

        if (WiFi.waitForConnectResult() != WL_CONNECTED) {
          Serial.printf("connection to WiFi failed!\n");
            
        }
        else{
          Serial.println();
          myIP = WiFi.localIP().toString();
          Serial.print("connected with IP: "); Serial.println(myIP);
          /////////////////////////////////////
          digitalWrite(IND_PIN,HIGH);
          delay(1000);
          digitalWrite(IND_PIN,LOW);
          /////////////////////////////////////
          wifi_channel = WiFi.channel();
          server_setup();
        }
      }
      else{
        Serial.println("wrong settings found...");
      }
    }
    }
    else{
      Serial.println("no saved info found...");
    }
}


int democount=0; 
void loop() {

   while (true) {

  
      // IR_INPUT();

      Serial.print("Slave Mac Adddress: ");Serial.println(slave_mac_address);
      Serial.print("Password: ");Serial.println(password);
      
      if ( (millis() - time1) > 8000 ) {
        op = 0;//Serial.println("no change");
      }
      if ( (millis() - time3) > 3000 ) {
        tmp_req = true;
        Serial.println("request temperatures");
      }

      // if(WiFi.status()== WL_CONNECTED){
        SCFM();
        SCH_ROUTINE();
        MANUAL_OP();
      // }

      // SCFM();
      // // RCFM();
      // SCH_ROUTINE();MANUAL_OP();
      if(RF_FLAG == false && configflag == true){
        loopFunction();
      }
      Serial.println("loop"); 
      Serial.print("Pot_Pin = ");Serial.println(analogRead(potPin));
      Serial.print("Minpot = ");Serial.println(minpot);
      Serial.print("Maxpot = ");Serial.println(maxpot);

      // delay(100);  // Delay for 1 millisecond

      yield();
    }

}
