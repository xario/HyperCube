#include <NTPClient.h>
#include <WiFiUdp.h>

#define LEAP_YEAR(Y)     ( (Y>0) && !(Y%4) && ( (Y%100) || !(Y%400) ) )

const long utcOffsetInSeconds = 0;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

unsigned long getEpochTime() {
  return timeClient.getEpochTime();
}

unsigned long secondsSince(String isoTime) {
  struct tm tm = {0};
  
  strptime(isoTime.c_str(), "%Y-%m-%dT%H:%M:%S.000S", &tm);
  return getEpochTime() - mktime(&tm);
}

String getISO8601Time(unsigned long timestamp = 0) {
  unsigned long rawTime = timestamp > 0 ? timestamp : timeClient.getEpochTime();
  unsigned long rawDays = rawTime / 86400L; // in days
  unsigned long days = 0, year = 1970;
  uint8_t month;
  static const uint8_t monthDays[]={31,28,31,30,31,30,31,31,30,31,30,31};

  while((days += (LEAP_YEAR(year) ? 366 : 365)) <= rawDays) {
    year++;
  }
    
  rawDays -= days - (LEAP_YEAR(year) ? 366 : 365); // now it is days in this year, starting at 0
  days=0;
  for (month=0; month<12; month++) {
    uint8_t monthLength;
    if (month==1) { // february
      monthLength = LEAP_YEAR(year) ? 29 : 28;
    } else {
      monthLength = monthDays[month];
    }
    if (rawDays < monthLength) break;
    rawDays -= monthLength;
  }

  unsigned long hours = (rawTime % 86400L) / 3600;
  String hoursStr = hours < 10 ? "0" + String(hours) : String(hours);

  unsigned long minutes = (rawTime % 3600) / 60;
  String minuteStr = minutes < 10 ? "0" + String(minutes) : String(minutes);

  unsigned long seconds = rawTime % 60;
  String secondStr = seconds < 10 ? "0" + String(seconds) : String(seconds);

  String monthStr = ++month < 10 ? "0" + String(month) : String(month); // jan is month 1  
  String dayStr = ++rawDays < 10 ? "0" + String(rawDays) : String(rawDays); // day of month  
  return String(year) + "-" + monthStr + "-" + dayStr + "T" + hoursStr + ":" + minuteStr + ":" + secondStr + ".000Z";
}

bool isNTPReady() {
  return timeClient.getEpochTime() > 1636242315;
}

void initNtp() {
  timeClient.begin();
}

void handleNTP() {
  timeClient.update();
}
