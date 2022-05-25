#include <ArduinoLowPower.h>
const int unit_number = 1; // unique ID for this unit (0<n<=999)
const int node_number = 0; // unique ID for the node for this unit

unsigned const int num_sleepcylces = 1; // number of 1-second sleep cycles between readings

// global variables
unsigned int i;
unsigned int j;
unsigned int jmax = 1;
unsigned int max_failcount = 5; // maximum number of failed transmissions before giving up;
unsigned int checksum_precision = 1000; // precision for checksum;

bool show_temp = 0;
bool show_moist = 1;

// set up soil moisture probes
unsigned const int soil_mosit_probe_number = 10;
int soil_moist_outputs[soil_mosit_probe_number];
unsigned const int soil_moist_sensors[soil_mosit_probe_number] = {3, 4, 5, 6, 7, 8, 9, 10, 13, 14}; 

// set up air temp/humidity
//#include <DHT.h> // header for air probes
unsigned const int max_tries = 10; // maximum tries for DHT sensor read
unsigned const int air_probe_number = 2;
int air_humid_outputs[air_probe_number];
int air_temp_outputs[air_probe_number];
uint8_t air_sensors[air_probe_number] = {0, 1};
//DHT dht[] = {
//  {air_sensors[1], DHT22},
//  {air_sensors[2], DHT22},
//};

// set up soil temp
unsigned const int soil_temp_probe_number = 10;
#include <OneWireNg.h>
#include <DallasTemperature.h>
#define ONE_WIRE_BUS 2
#define TEMPERATURE_PRECISION 12
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress deviceAddress[soil_temp_probe_number];
int soil_temp_outputs[soil_temp_probe_number];

// set up RTC
#include "RTClib.h"
RTC_DS3231 rtc;
int rtc_month;
int rtc_day;
int rtc_hour;
int rtc_minute;
int rtc_second;
int rtc_temp;

// battery check
int battery_voltage;

// set up arrays for data storage
const int nrecords = 100; // number of local records to save (must be >=2)

int data_air_temp[nrecords][air_probe_number];
int data_air_humid[nrecords][air_probe_number];
int data_soil_temp[nrecords][soil_temp_probe_number];
int data_soil_moist[nrecords][soil_mosit_probe_number];
int data_battery[nrecords];
int data_time[nrecords][4];
int data_rtc_temp[nrecords];

int samp_position = 0;  // position in data array
int flash_position = 0; // marker for saved status on flash chip
int trans_position = nrecords; // last record transmitted - "nrecords" indicates that no data have been sent yet
unsigned long total_samples = 0; // counter tracking total number of samples sent


// LoRa Settings
#include <LoRa.h>
#include <SPI.h>

const long freq = 868E6; // for Europe - set to 915E6 for North America
const int SF = 9;

void setup()
{ 
   Serial.begin(9600);
   randomSeed(unit_number); // set seed for unique random sequences

   // soil moisture
   for(i = 0; i < soil_mosit_probe_number; i++) {
    pinMode(soil_moist_sensors[i], OUTPUT);
    digitalWrite(soil_moist_sensors[i], LOW);
   }

   // air
   //for(i = 0; i < air_probe_number; i++) {
   //    dht[i].begin();
   //}

   // soil temp
   sensors.begin();
  
   // set resolution for each device
   for(i = 0; i <soil_temp_probe_number; i++) {
    sensors.getAddress(deviceAddress[i], i);
    sensors.setResolution(deviceAddress[i], TEMPERATURE_PRECISION);
   }

   // RTC
   pinMode(A2, OUTPUT);
   digitalWrite(A2, HIGH);
   delay(1000);
   rtc.begin();
   if (rtc.lostPower()) { // reset time if unit battery ran out
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
   }
   digitalWrite(A2, LOW);

   // battery check
   pinMode(A6, OUTPUT);
   digitalWrite(A6, LOW);

   // set up LoRa
   LoRa.begin(freq); // frequency (c
   LoRa.setSpreadingFactor(SF);  // TODO: think about best SF value
}

void loop()
{
  LoRa.sleep(); // start in sleep mode
  
  //Serial.print("Measurement number: ");
  //Serial.println(samp_position);
  //Serial.println("measuring soil moisture...");
  // soil moisture
  for(i = 0; i < soil_mosit_probe_number; i++) {
    digitalWrite(soil_moist_sensors[i], HIGH);
    delay(200);            // waits for a second
    soil_moist_outputs[i] = analogRead(A0);
    digitalWrite(soil_moist_sensors[i], LOW);
  }

  //Serial.println("measuring soil temp...");
  // soil temp
  delay(200);
  sensors.requestTemperatures();
  for(i = 0; i <soil_temp_probe_number; i++) {
     soil_temp_outputs[i] = round(sensors.getTempC(deviceAddress[i])*100);
  }

  //Serial.println("saving results...");
  // save results
  for(i = 0; i < soil_temp_probe_number; i++) {
    data_soil_temp[samp_position][i] = soil_temp_outputs[i];
  }
  for(i = 0; i < soil_mosit_probe_number; i++) {
    data_soil_moist[samp_position][i] = soil_moist_outputs[i];
  }
  
  data_time[samp_position][0] = rtc_month;
  data_time[samp_position][1] = rtc_day;
  data_time[samp_position][2] = rtc_hour;
  data_time[samp_position][3] = rtc_minute;
  data_rtc_temp[samp_position] = rtc_temp;

  // increment sample position
  j = samp_position;
  samp_position = (samp_position + 1) % nrecords; // circular array
  total_samples++;
  
  //Serial.println("printing results...");
  if(Serial) {
    if(total_samples > samp_position) {
      // we have already gone around the circular array at least once.
      // read out all records
      jmax = nrecords;
    }
    else {
      // we have not yet gone around the circular array at least once.
      // read out only to samp_position.
      jmax = samp_position;
    }
    //Serial.print("Max samples: ");
    //Serial.println(total_samples);

    //for(j = 0; j < jmax; j++) {
      //Serial.print("Record number: ");
      //Serial.println(j);
    
      // print soil moisture
      if(show_moist) {
        for(i = 0; i < soil_mosit_probe_number; i++) {
          Serial.print("SM");
          Serial.print(soil_moist_sensors[i]);
          Serial.print(": ");
          Serial.print(data_soil_moist[j][i]);
          Serial.print("; ");
        }
        Serial.println();
      }
    
      // print soil temp
      if(show_temp) {
        for(i = 0; i < soil_temp_probe_number; i++) {
          Serial.print("ST");
          Serial.print(i);
          Serial.print(": ");
          Serial.print(data_soil_temp[j][i]);
          Serial.print("; ");
        }
        Serial.println();
      }
    }
    //Serial.println();
  //}
}


long checksum_fun(int trans_position) {
  long checksum = 0;
  for(i = 0; i < air_probe_number; i++) {
    checksum = checksum + data_air_temp[trans_position][i];
    checksum = checksum + data_air_humid[trans_position][i];
  }
  for(i = 0; i < soil_temp_probe_number; i++) {
    checksum = checksum + data_soil_temp[trans_position][i];
  }
  for(i = 0; i < soil_mosit_probe_number; i++) {
    checksum = checksum + data_soil_moist[trans_position][i];
  }
  checksum = checksum + data_battery[trans_position];
  
  checksum = checksum + data_time[trans_position][0];
  checksum = checksum + data_time[trans_position][1];
  checksum = checksum + data_time[trans_position][2];
  checksum = checksum + data_time[trans_position][3];
  checksum = checksum + data_rtc_temp[trans_position];
  checksum = checksum + unit_number;

  return(checksum);
}
