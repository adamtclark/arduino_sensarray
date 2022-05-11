#include <ArduinoLowPower.h>
const int unit_number = 1; // unique ID for this unit (0<n<=999)
const int node_number = 1; // unique ID for the node for this unit (0<n<=999)
const int scaling_factor = 1E3; // scaling factor for separating unit and node IDs
String sensorID = "SensorID:" + String(node_number*scaling_factor+unit_number);

unsigned const int num_sleepcylces = 10;// 75; // number of 8-second sleep cycles between readings; 75 = 10 minutes

// global variables
unsigned int i;
unsigned int j;
unsigned int jmax = 1;
unsigned int max_failcount = 5; // maximum number of failed transmissions before giving up;

// set up soil moisture probes
unsigned const int soil_mosit_probe_number = 10;
int soil_moist_outputs[soil_mosit_probe_number];
unsigned const int soil_moist_sensors[soil_mosit_probe_number] = {3, 4, 5, 6, 7, 8, 9, 10, 13, 14}; // pins for sensors 

// set up air temp/humidity
#include <DHT.h> // header for air probes
unsigned const int max_tries = 10; // maximum tries for DHT sensor read
unsigned const int air_probe_number = 2;
int air_humid_outputs[air_probe_number];
int air_temp_outputs[air_probe_number];
uint8_t air_sensors[air_probe_number] = {0, 1};
DHT dht[] = {
  {air_sensors[1], DHT22},
  {air_sensors[2], DHT22},
};

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
const int nrecords = 150; // number of local records to save (must be >=2)

int data_air_temp[nrecords][air_probe_number];
int data_air_humid[nrecords][air_probe_number];
int data_soil_temp[nrecords][soil_temp_probe_number];
int data_soil_moist[nrecords][soil_mosit_probe_number];
int data_battery[nrecords];
int data_time[nrecords][4];
int data_rtc_temp[nrecords];

int samp_position = 0;  // position in data array
int trans_position = nrecords; // last record transmitted - "nrecords" indicates that no data have been sent yet
unsigned long total_samples = 0; // counter tracking total number of samples sent


// LoRa Settings
#include <LoRa.h>
#include <SPI.h>

const long freq = 868E6; // for Europe - set to 915E6 for North America
const int SF = 9; // TODO: think about this
const int send_interval = 500; // number of milliseconds between sending events
const int numtries = 10; // number of tries before taking a break

int trans_start_time = 0; // begin time of day to send transmission
int trans_end_time = 1; // end time of day to send transmission
bool trans_sent_today = 0; // trigger for controling transmission status



// Flash Settings
#include <SparkFun_SPI_SerialFlash.h>
const byte PIN_FLASH_CS = 32; // Change this to match the Chip Select pin on your board
SFE_SPI_FLASH myFlash;
int separator = 9999;
long flash_position = 0; // marker for saved status on flash chip
long flash_size = 2000000;

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
   for(i = 0; i < air_probe_number; i++) {
       dht[i].begin();
   }

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
   if (rtc.lostPower()) { // reset time if unit battery ran out (to time of last compiling!!)
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
   }
   digitalWrite(A2, LOW);

   // battery check
   pinMode(A6, OUTPUT);
   digitalWrite(A6, LOW);

   // set up LoRa
   LoRa.begin(freq); // Set LoRa frequency
   LoRa.setSpreadingFactor(SF);  // TODO: think about best SF value
}

void loop()
{
  LoRa.sleep(); // start in sleep mode
  
  Serial.print("Measurement number: ");
  Serial.println(samp_position);
  Serial.println("measuring soil moisture...");
  // soil moisture
  for(i = 0; i < soil_mosit_probe_number; i++) {
    digitalWrite(soil_moist_sensors[i], HIGH);
    delay(1000);            // waits for a second
    soil_moist_outputs[i] = analogRead(A0);
    digitalWrite(soil_moist_sensors[i], LOW);
  }

  Serial.println("measuring air...");
  // air temp and humid
  for(i = 0; i < air_probe_number; i++) {
    int n;
    //Read data and store it to variables hum and temp
    n = 0;
    air_humid_outputs[i] = round(dht[i].readHumidity()*100);
    while (isnan(air_humid_outputs[i]) & n < max_tries) {
      delay(1000);
      air_humid_outputs[i] = round(dht[i].readHumidity()*100);
      n++;
    }
    n = 0;
    air_temp_outputs[i] = round(dht[i].readTemperature()*100);
    while (isnan(air_temp_outputs[i]) & n < max_tries) {
      delay(1000);
      air_temp_outputs[i] = round(dht[i].readTemperature()*100);
      n++;
    }
  }

  Serial.println("measuring soil temp...");
  // soil temp
  delay(1000);
  sensors.requestTemperatures();
  for(i = 0; i <soil_temp_probe_number; i++) {
     soil_temp_outputs[i] = round(sensors.getTempC(deviceAddress[i])*100);
  }

  Serial.println("measuring battery...");
  // battery level
  digitalWrite(A6, HIGH);
  delay(2000);
  analogReference(AR_INTERNAL);
  for(i = 0; i < 10; i++) {
    battery_voltage = analogRead(A1);
    delay(100);
  }
  battery_voltage = analogRead(A1);

  analogReference(AR_DEFAULT);
  for(i = 0; i < 10; i++) {
    analogRead(A1);
    delay(100);
  }
  digitalWrite(A6, LOW);

  Serial.println("measuring time...");
  // RTC
  digitalWrite(A2, HIGH);
  delay(5000);
  DateTime now = rtc.now();
  rtc_temp = round(rtc.getTemperature()*100);
  digitalWrite(A2, LOW);
  rtc_month = now.month();
  rtc_day = now.day();
  rtc_hour = now.hour();
  rtc_minute = now.minute();
  rtc_second = now.second();

  Serial.println("saving results...");
  // save results
  for(i = 0; i < air_probe_number; i++) {
    data_air_temp[samp_position][i] = air_temp_outputs[i];
    data_air_humid[samp_position][i] = air_humid_outputs[i];
  }
  for(i = 0; i < soil_temp_probe_number; i++) {
    data_soil_temp[samp_position][i] = soil_temp_outputs[i];
  }
  for(i = 0; i < soil_mosit_probe_number; i++) {
    data_soil_moist[samp_position][i] = soil_moist_outputs[i];
  }
  data_battery[samp_position] = battery_voltage;
  
  data_time[samp_position][0] = rtc_month;
  data_time[samp_position][1] = rtc_day;
  data_time[samp_position][2] = rtc_hour;
  data_time[samp_position][3] = rtc_minute;
  data_rtc_temp[samp_position] = rtc_temp;

  // print outputs // TODO: silence this
  Serial.println("printing results...");
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
    Serial.print("Max samples: ");
    Serial.println(total_samples);
    for(j = 0; j < jmax; j++) {
      Serial.print("Record number: ");
      Serial.println(j);
      
      // print time
      Serial.print("Time: ");
      Serial.print(data_time[j][0]);
      Serial.print('/');
      Serial.print(data_time[j][1]);
      Serial.print('-');
      Serial.print(data_time[j][2]);
      Serial.print(':');
      Serial.print(data_time[j][3]);
      Serial.print("; Temp: ");
      Serial.print(data_rtc_temp[j]);
      Serial.println();
    
      // print soil moisture
      for(i = 0; i < soil_mosit_probe_number; i++) {
        Serial.print("SM");
        Serial.print(soil_moist_sensors[i]);
        Serial.print(": ");
        Serial.print(data_soil_moist[j][i]);
        Serial.print("; ");
      }
      Serial.println();
    
      // print air humid
      for(i = 0; i < air_probe_number; i++) {
        Serial.print("AH");
        Serial.print(air_sensors[i]);
        Serial.print(": ");
        Serial.print(data_air_humid[j][i]);
        Serial.print("; ");
      }
    
      // print air temp
      for(i = 0; i < air_probe_number; i++) {
        Serial.print("AT");
        Serial.print(air_sensors[i]);
        Serial.print(": ");
        Serial.print(data_air_temp[j][i]);
        Serial.print("; ");
      }
      Serial.println();
    
      // print soil temp
      for(i = 0; i < soil_temp_probe_number; i++) {
        Serial.print("ST");
        Serial.print(i);
        Serial.print(": ");
        Serial.print(data_soil_temp[j][i]);
        Serial.print("; ");
      }
      Serial.println();
    
      // print battery voltage;
      Serial.print("BVolt: ");
      Serial.println(data_battery[j]);
    }
    Serial.println();
  }

  // Transmit over LoRa
  int tmp_time = data_time[samp_position][2]; // current hour, from RTC
  if((tmp_time > trans_start_time) & (tmp_time < trans_end_time) & (trans_sent_today ==0)) { // only run from midnight to 1 AM.
    // Set up for transmission
    trans_position = nrecords;
    bool node_connected = 0; // no sensor connected yet
    int n = 0;
    long startTime;
    int tmp_trans_position; // temporary position for sending data

    // Loop: keep trying to transmit for the full time window, until successful
    while((tmp_time > trans_start_time) & (tmp_time < trans_end_time) & (trans_sent_today ==0)) {
      // Connect to Node
      Serial.println("Waiting for FREE... ");
      String output_LoRa = "";
      startTime = millis();        // time of last packet send
      while(!node_connected & ((millis() - startTime) < (2*send_interval))) { // receive for 2 send intervals, or until signal received
        int packetSize = LoRa.parsePacket();
        if (packetSize) {
          Serial.println("Signal Detected.");
    
          // TIME 0-?: FREE signal obtained
          output_LoRa = getFREE();
          if(output_LoRa == ("FREE"+(String)node_number)) {
            Serial.print("Node Detected. ID ");
            Serial.println(output_LoRa);
    
            // note - this must be random in case multiple sensors send their signals at once
            delay(random(0.4*send_interval) + 0.8*send_interval); // wait for 0.8 to 1.2 send_interval milliseconds
    
            // TIME 1-2: ID Sent
            sendID(); // lasts 1 send interval
    
            // TIME 2-5: Check ID
            node_connected = checkID(); // lasts 3 send intervals
          }
        }
        else {
          delay(random(send_interval/5));
        }
      }
    
      // Connection successful - send data
      bool all_signals_received = 0; // has transmission caught up to sampling?
      if(node_connected) {
        // connection successful - send data
        Serial.print("Sample position: ");
        Serial.println(samp_position);
        n = 0; // start counter for maximum tries
        
        while(!all_signals_received  & n < numtries) {
          bool signal_received = 0;
          if(trans_position == nrecords) { // no records have been sent yet
            tmp_trans_position = 0;
          }
          else {
            tmp_trans_position = ((trans_position+1) % nrecords); // record to attempt to send
          }
          
          Serial.print("Transmitting record number: ");
          Serial.println(tmp_trans_position);
          while(!signal_received & n < numtries) {
            // TIME 0: Send Data
            startTime = millis();        // time of last packet send
            while((millis() - startTime) < (send_interval)) { // send for send_interval milliseconds
              sendData_fun(tmp_trans_position);
              delay(random(send_interval/10));
            }
    
            // TIME 1: Check Data
            startTime = millis();        // time of last packet send
            while((millis() - startTime) < (2*send_interval)) { // receive for 2*send_interval milliseconds
              if(signal_received == 0) { // only run if signal not yet checked
                signal_received = checkData(signal_received);
                delay(random(send_interval/10));
              }
            }
        
            if(!signal_received) {
              Serial.println("Sending Failed.");
              n++;
            }
        
            if(n >= numtries) {
              Serial.println("Sending Failed - ending attempts.");
            }
    
            // note on timing: worst case, both sensors are receiving and sending at the same time
            // need a shift of 1 send_interval to fix this on average
            // thus, the average waiting intervals here must be longer, and should be different from
            // those in the reciever code
            delay(random(send_interval/5));
          }
          if(signal_received) {
            n = 0; // reset try counter
            trans_position = tmp_trans_position; // increment transfer counter
            if(trans_position == (nrecords-1)) {
              all_signals_received = 1;
              Serial.println("All Records Successfully Received.");
            }
          }
        }
      }
      else {
        Serial.println("Connection Failed - ending attempts.");
      }
    
      if(node_connected) {
        Serial.println("Sending Done.");
        startTime = millis();        // time of last packet send
        while(millis() - startTime < 2*send_interval) { // send Done signal for 2*send_interval milliseconds
          sendDone(); // Send message that sending is done
          delay(random(send_interval/10));
        }
        
        trans_sent_today = 1; // set trigger that tranmission has been sent today
      }
    
      if(!node_connected | !all_signals_received) {
        // transfer failed - go to sleep and try again
        LoRa.sleep();
        Serial.println("Connection and/or Sending Failed. Sleeping...");
        Serial.println();
        delay(random(send_interval) + 2*send_interval); // wait 2-3 send_intervals
      }
     
      Serial.println("End of transmission attempt.");
      Serial.println();
      LoRa.sleep();
      delay(random(6000, 8000)); // wait 6-8 seconds

      // Get current time
      digitalWrite(A2, HIGH);
      delay(5000);
      now = rtc.now();
      digitalWrite(A2, LOW);
      tmp_time = now.hour();
    }
  }
  else {
    // outside of transmission time - reset trigger
    trans_sent_today = 0;  
  }

  // Save to flash
  long ilong;
  // set up flash
  Serial.println("Saving to Flash. Turning off LoRa.");
  pinMode(LORA_RESET, OUTPUT);  //LORA reset pin declaration as output
  digitalWrite(LORA_RESET, LOW);  //turn off LORA module
  Serial.println("LoRa Off.");
  delay(500);

  Serial.print("Beginning Flash. Status: ");
  bool flashstart = myFlash.begin(PIN_FLASH_CS, 2000000, SPI1);
  Serial.println(flashstart);

  // If first run, or a reboot, find starting position in flash
  if(flash_position==0) {
    Serial.println("Finding start position.");
    ilong = 0;
    int numzeros = 0;
    byte val[3];
    // looking for three 0xFF blocks in a row
    while((ilong < flash_size) & (numzeros<3)) {
      val[numzeros] = myFlash.readByte(ilong);
      if(val[numzeros] == 0xFF) {
        numzeros++;
      }
      else {
        numzeros = 0;
      }
      
      ilong++;
    }
    if((ilong >= 3) & (ilong<flash_size)) {
      flash_position = ilong-3;
    } else {
      flash_position = 0; // when in doubt, start at zero
    }
    Serial.print("Start position: ");
    Serial.println(flash_position);
  }

  Serial.println("Saving Data.");
  
  // save data
  Serial.print("Sample position:");
  Serial.println(samp_position);
  j = samp_position;
  
  for(i = 0; i < air_probe_number; i++) {
    myFlash.writeBlock(flash_position, (uint8_t*)&(data_air_temp[j][i]), 2);
    flash_position = (flash_position+2)%flash_size;
  }
  for(i = 0; i < air_probe_number; i++) {
    myFlash.writeBlock(flash_position, (uint8_t*)&(data_air_humid[j][i]), 2);
    flash_position = (flash_position+2)%flash_size;
  }
  for(i = 0; i < soil_temp_probe_number; i++) {
    myFlash.writeBlock(flash_position, (uint8_t*)&(data_soil_temp[j][i]), 2);
    flash_position = (flash_position+2)%flash_size;
  }
  for(i = 0; i < soil_mosit_probe_number; i++) {
    myFlash.writeBlock(flash_position, (uint8_t*)&(data_soil_moist[j][i]), 2);
    flash_position = (flash_position+2)%flash_size;
  }
  myFlash.writeBlock(flash_position, (uint8_t*)&(data_battery[j]), 2);
  flash_position = (flash_position+2)%flash_size;

  for(i = 0; i < 4; i++) {
    myFlash.writeBlock(flash_position, (uint8_t*)&(data_time[j][i]), 2);
    flash_position = (flash_position+2)%flash_size;
  }
  myFlash.writeBlock(flash_position, (uint8_t*)&(data_rtc_temp[j]), 2);
  flash_position = (flash_position+2)%flash_size;
  myFlash.writeBlock(flash_position, (uint8_t*)&(unit_number), 2);
  flash_position = (flash_position+2)%flash_size;

  myFlash.writeBlock(flash_position, (uint8_t*)&(separator), 2);
  flash_position = (flash_position+2)%flash_size;

  Serial.println("Flash Saving Done.");
  Serial.print("Flash Position: ");
  Serial.println(flash_position);

  // increment sample position
  samp_position = (samp_position + 1) % nrecords; // circular array
  total_samples++;
  
  // Go to sleep and wait for next reading
  Serial.println("sleep...");
  // Save power
  USBDevice.detach();
  for(i = 0; i < num_sleepcylces; i++) {
    LowPower.sleep(8000);
  }
  USBDevice.attach();
  Serial.begin(9600);
  Serial.println("wake up...");
  Serial.println();
}


String getFREE() {
  // read packet
  String output_LoRa = "";
  while (LoRa.available()) {
    output_LoRa = output_LoRa+(char)LoRa.read();
  }
  return(output_LoRa);
}

void sendID() {
  long startTime = millis();        // time of last packet send
  while(millis() - startTime < send_interval) {
    LoRa.beginPacket();
    LoRa.print(sensorID);
    LoRa.endPacket();
    delay(send_interval/random(100)); // TODO: switch to 10?
  }
}

bool checkID() {
  bool node_connected = 0;
  long startTime = millis();        // time of last packet send
  while(millis() - startTime < (3*send_interval)) {
    if(!node_connected) {
      int packetSize = LoRa.parsePacket();
      if (packetSize) {
        // read packet
        String output_LoRa = "";
        while (LoRa.available()) {
          output_LoRa = output_LoRa+(char)LoRa.read();
        }
        Serial.print("Lora Output is: ");
        Serial.println(output_LoRa);
        
        if(sensorID ==output_LoRa) {
          Serial.print("Successfully coupled. Node ");
          Serial.println(output_LoRa);
          node_connected = 1;
        }
      }
    }
    delay(random(send_interval/10));
  }
  return(node_connected);
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

void sendData_fun(int trans_position) {
  long checksum_out = checksum_fun(trans_position);  
  LoRa.beginPacket();
  if(trans_position==0) {
    LoRa.write((uint8_t *)&(nrecords), 2);
  } else {
    LoRa.write((uint8_t *)&(trans_position), 2);
  }
  //int tmpV;
  //uint8_t* tmpP;
  for(i = 0; i < air_probe_number; i++) {
    //tmpV = data_air_temp[trans_position][i];
    //tmpP = (uint8_t*)tmpV;
    //LoRa.write(tmpP, 2);
    LoRa.write((uint8_t *)&(data_air_temp[trans_position][i]), 2);
  }
  for(i = 0; i < air_probe_number; i++) {
    LoRa.write((uint8_t*)&(data_air_humid[trans_position][i]), 2);
  }

  for(i = 0; i < soil_temp_probe_number; i++) {
    LoRa.write((uint8_t*)&(data_soil_temp[trans_position][i]), 2);
  }
  for(i = 0; i < soil_mosit_probe_number; i++) {
    LoRa.write((uint8_t*)&(data_soil_moist[trans_position][i]), 2);
  }
  LoRa.write((uint8_t*)&(data_battery[trans_position]), 2);
  for(i = 0; i < 4; i++) {
    LoRa.write((uint8_t*)&(data_time[trans_position][i]), 2);
  }
  LoRa.write((uint8_t*)&(data_rtc_temp[trans_position]), 2);
  LoRa.write((uint8_t*)&(unit_number), 2);
  LoRa.write((uint8_t*)&(checksum_out), 4);
  
  LoRa.endPacket();
}

bool checkData(bool signal_received) {
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    // read packet
    String output_LoRa = "";
    while (LoRa.available()) {
      output_LoRa = output_LoRa+(char)LoRa.read();
    }
    Serial.print("Output is: ");
    Serial.println(output_LoRa);
    
    if(output_LoRa == ("Success"+(String)node_number)) {
      Serial.print("Data successfully checked. ms ");
      Serial.println(millis());
      signal_received = 1;
    }
  }
  return(signal_received);
}

void sendDone() {
  int catch_done = 9999; // variable for signalling that data has all been sent
  LoRa.beginPacket();
  LoRa.write((uint8_t*)&(catch_done), 2);
  LoRa.endPacket();
}
