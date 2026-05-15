#include <HTTPClient.h>
#include "time.h"

void updateTimeNow() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
#ifdef DEBUG
    Serial.println("Failed to obtain time");
#endif
    return;
  }
  // Format the time into WMMDDYYHHMMSS
  char buffer[16];

  // Convert weekDayday to numeric value (1=Monday, 2=Tuesday, ...)
  weekDay = timeinfo.tm_wday - 1;  // tm_wday is 0=Sunday, 1=Monday, ..., 6=Saturday
  current_hour = timeinfo.tm_hour;

  // Format the string with leading zeroes if needed
  snprintf(buffer, sizeof(buffer), "%02d%02d%02d%02d%02d%02d",
           timeinfo.tm_mday,
           timeinfo.tm_mon + 1,     // Month is 0-based, so add 1
           timeinfo.tm_year % 100,  // Year is years since 1900, so take the last two digits
           timeinfo.tm_hour,
           timeinfo.tm_min,
           timeinfo.tm_sec);

  timenow = String(buffer);
  mm = timeinfo.tm_mon + 1;
  dd = timeinfo.tm_mday;
  yy = timeinfo.tm_year % 100;
  hh = timeinfo.tm_hour;
  mins = timeinfo.tm_min;
  ss = timeinfo.tm_sec;
#ifdef DEBUG
  Serial.print("Sending timenow: ");
  Serial.println(timenow);
#endif
  switch (mm) {
    case 1:
      month = "Jan";
      break;
    case 2:
      month = "Feb";
      break;
    case 3:
      month = "Mar";
      break;
    case 4:
      month = "Apr";
      break;
    case 5:
      month = "May";
      break;
    case 6:
      month = "Jun";
      break;
    case 7:
      month = "Jul";
      break;
    case 8:
      month = "Aug";
      break;
    case 9:
      month = "Sep";
      break;
    case 10:
      month = "Oct";
      break;
    case 11:
      month = "Nov";
      break;
    case 12:
      month = "Dec";
    default:
      month = "Jan";
      break;
  }
}
