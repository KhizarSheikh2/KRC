#ifndef EXTERN_VARS_H
#define EXTERN_VARS_H

extern int device_type;
extern String savedatacommand;
extern String lastcmd;
extern int disconnect_wifi;
extern int switchblecounter;
extern String dataarray;
extern String deviceinfo;
extern String dataarraydashboard;
extern String myID;
extern int retry_count;
extern String lastcmd;
extern int forceBLE;
extern String commandstring;
extern String message_string;
extern int commandrcvd;
extern long packet_sequence;
extern String myIP;
extern String timenow;
extern String timesch;
extern uint8_t timeschen,prevtimeschen; 
extern String alarm_string;
extern RTC_DATA_ATTR int counter;

extern int cloudflag;

extern uint8_t broadcastAddress[6];
extern String slave_mac_address;
extern String ble_slave_mac_address;

//========================================================================================================//
extern uint8_t pos,pos1,pos2,Pos,Pos1,comma_index[25],Time_data[32],SchScan[8],formate,values[10],scfm[4],rcfm[4];
extern String SchString,TimeString[3],STR_buffer,deviceTime;
extern float MaxDegree,celsius;
// extern int Minfb;
extern byte i,present,type_s,data[12],addr[8],LastState; 
extern int temper,tprev,count,BYPASS_POS,BYPASS_STATUS,RDEGREE,DEGREE,SP,prevSP,spos,rpos;
extern bool op,SHED,SCH_update,supcfm_update,retcfm_update;
extern unsigned long time1;
//========================================================================================================//
extern String dmptemp;
extern String dmptempsp;
extern uint8_t dampertsw,damperswitch,prevdampertsw;
extern String supcfm; 
extern String retcfm;
extern uint8_t dampstate;

extern int closeddamp;
extern int damptsw;
extern String cfm;

extern uint8_t seasonsw,prevseasonsw;
extern String temp1;extern String temp2;
extern String temp1sp; extern String temp2sp;

extern int exflg;
extern int data_ok;

extern int minpot,maxpot;
#endif
