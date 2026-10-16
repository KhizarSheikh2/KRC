#ifndef VARIABLES_H
#define VARIABLES_H

#include <WiFi.h>
#include <ArduinoJson.h>
#include <esp_task_wdt.h>
#include "EEPROM.h"
#include <String>

#define WDT_TIMEOUT 60
#define EEPROM_SIZE 128
String Name = "ZoneMaster";
int day_app[7]={5,6,7,1,2,3,4};
int for_schrout[8]={0,3,4,5,6,7,1,2};
RTC_DATA_ATTR int counter=0;
int tmp;
bool tmp_req=false;
unsigned long time3=0;
String ssid = "";
String password = "";
int commandrcvd=0;
long packet_sequence=0;
int disconnect_wifi =0;
int modeIdx=0;
String dataarray =  "";
String dataarraydashboard ="";
String savedatacommand = "";
String deviceinfo = "";
bool deviceConnected = false;
String commandstring = "";
String commandresult = "";
String message_string ="";
//--------------------------------------------------------------
//DEVICE DEFAULTS SAVED HERE
//--------------------------------------------------------------
uint8_t newMACAddress[] = {0x11, 0x18, 0xC2, 0x06, 0x5B, 0x6C};
String myIP="";
String myID= "16092100125";
String substrin1=myID.substring(2,6);
String substrin2=myID.substring(9,11);
String hostname= Name+substrin1+substrin2;
String devicename="ZMR-01";
String devversion="1.0";
int device_type = 3;
int wifi_channel=0;
long switchblecounter= 5; //Try WIFI 10 times before turning on BLE
//========================================================================================================//
uint8_t pos,pos1,pos2,Pos,Pos1,comma_index[25],Time_data[32],SchScan[8],formate,values[10],scfm[4],rcfm[4];
String SchString,TimeString[3],STR_buffer,deviceTime;
float MaxDegree = 1.68,celsius;
// int Minfb = 3450;
byte i,present = 0,type_s,data[12],addr[8],LastState = 0; 
int temper,tprev = 0,count = 0,BYPASS_POS,BYPASS_STATUS,RDEGREE = 1.5,DEGREE = 1.5,SP=0,prevSP=0,spos,rpos;
bool op= 0,SHED = 0,SCH_update = 0,supcfm_update = 0,retcfm_update = 0;
unsigned long time1;
//========================================================================================================//
String sdata="";
int connection_mode=0;
int retry_count =10;//HTTP Begin retry count
int blink=0;
hw_timer_t * timer = NULL; 
volatile bool datainterrupt = false;
long SamplingRate = 1000; //Read 1000 values in one second.
long milscount =0;
long ledcounter =0;
int ledretrycount=0;
int wifi_status=0;
long disconnection_counter=300;//Once phone is connected to BLE, keep the connection for 300 seconds.
String macaddress = "";
String lastcmd = "";
int forceBLE=0;

String timenow = "010120000000";
String timesch = "hoursch=24|daysch=7";
uint8_t timeschen = 0,prevtimeschen = 0;
String alarm_string = "0";

String dmptemp="0";
String dmptempsp="0";
uint8_t dampertsw=0,damperswitch=0,prevdampertsw=0;
String supcfm="0-0"; 
String retcfm="0-0";
uint8_t dampstate=0;

int closeddamp=2;
int damptsw=0;
String cfm="0-0";

uint8_t seasonsw=0,prevseasonsw = 0;
String temp1="0";String temp2="0";
String temp1sp="0"; String temp2sp="0";


uint8_t broadcastAddress[6];
String slave_mac_address="";
String ble_slave_mac_address="";
int cloudflag=0,ir_state = 0;
int exflg = 0;
int data_ok = 0;

int minpot = 0,maxpot = 0;

#endif
