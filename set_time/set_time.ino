// Date and time functions using a PCF8523 RTC connected via I2C and Wire lib
#include "RTClib.h"
RTC_DS3231 rtc;

char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

void setup () {
  Serial.begin(9600);
  pinMode(A2, OUTPUT);
  digitalWrite(A2, HIGH);


  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    while (1) delay(10);
  }

  //if (rtc.lostPower()) {
    Serial.println("RTC is NOT initialized, let's set the time!");
    Serial.println(F(__TIME__));
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  //}
}

void loop () {
    DateTime now = rtc.now();
    int rtc_temp = round(rtc.getTemperature()*100);
    int rtc_month = now.month();
    int rtc_day = now.day();
    int rtc_hour = now.hour();
    int rtc_minute = now.minute();
    int rtc_second = now.second();

  
    Serial.print(now.year());
    Serial.print('/');
    Serial.print(rtc_month);
    Serial.print('/');
    Serial.print(rtc_day);
    Serial.print('-');
    Serial.print(rtc_hour);
    Serial.print(':');
    Serial.print(rtc_minute);
    Serial.print(':');
    Serial.print(rtc_second);
    Serial.println();

    delay(3000);
}
