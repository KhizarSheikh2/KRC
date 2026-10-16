#ifndef WIFI_THREAD_H
#define WIFI_THREAD_H
#include <Arduino.h>

void server_setup();
String httpGETRequest(const char* commandforserver);
String THERMhttpGETRequest(const char* serverName);

#endif
