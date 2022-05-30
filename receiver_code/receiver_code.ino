#include <LoRa.h>
#include <SPI.h>
#include <ArduinoLowPower.h>

const long freq = 868E6; // for Europe - set to 915E6 for North America
const int SF = 9;
const int node_number = 1; // unique ID for the node for this unit (0<n<=999)
const int scaling_factor = 1E3; // scaling factor for separating unit and node IDs
unsigned const int num_sleepcylces = 75; // number of 8-second sleep cycles between readings; 75 = 10 minutes

const int send_interval = 500;
const int numtries = 10; // number of tries before taking a break

int trans_start_time = 0; // begin time of day to send transmission
int trans_end_time = 1; // end time of day to send transmission

// save data
const int nrecords = 150; // number records

unsigned const int air_probe_number = 2;
unsigned const int soil_temp_probe_number = 10;
unsigned const int soil_mosit_probe_number = 10;

int save_air_temp[nrecords][air_probe_number];
int save_air_humid[nrecords][air_probe_number];
int save_soil_temp[nrecords][soil_temp_probe_number];
int save_soil_moist[nrecords][soil_mosit_probe_number];
int save_battery[nrecords];
int save_time[nrecords][4];
int save_rtc_temp[nrecords];
int save_unit_number[nrecords];
long save_checksum_out[nrecords];
long local_checksum_out;

// set up RTC
#include "RTClib.h"
RTC_DS3231 rtc;
int tmp_time = 99; // initialize outside of plausible range

// set up SD card
/*
   SD card attached to SPI bus as follows:
 ** MOSI/COPI - pin 8
 ** MISO/CIPO - pin 10
 ** SCK - pin 9
 ** CS - pin 4 (for MKRZero SD: SDCARD_SS_PIN) TODO: potentially try changing to 5?

  modified from David A. Mellis (Nov 2010) and Tom Igoe (Apr 2012)
*/
#include <SD.h>
File myFile;
int CS_pin = 4;

void setup() {
  Serial.begin(9600);
  
  Serial.println("LoRa Receiver");
  Serial.print("Node number:");
  Serial.println(node_number);

  LoRa.begin(freq); // Set LoRa frequency
  LoRa.setSpreadingFactor(SF);  // TODO: think about best SF value

  // RTC
  pinMode(A2, OUTPUT);
  digitalWrite(A2, HIGH);
  delay(1000);
  rtc.begin();
  if (rtc.lostPower()) { // reset time if unit battery ran out (to time of last compiling!!)
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  digitalWrite(A2, LOW);

  // Set up SD
  // TODO: Move into loop if necessary?
  Serial.print("SD Initializing SD card...");
  if (!SD.begin(CS_pin)) {
    Serial.println("initialization failed!");
  }
  else {
    Serial.println("initialization done.");
  }
}

void loop() {
  int i;
  int j;
  int catch_done;
  int packetSize;
  unsigned int n;
  delay(8000); 
  
  while((tmp_time >= trans_start_time) & (tmp_time < trans_end_time)) {
    n = 0;
  
    long startTime;
    String output_LoRa;
    
    // Connect to Sensor
    // Broadcast that the node is free to recieve
    bool sensor_connected = 0;
    // TODO: add in break condition in case sensor is not connected?
    while(!sensor_connected & (tmp_time >= trans_start_time) & (tmp_time < trans_end_time)) {
      // TIME 0-1: FREE Sent
      Serial.println("Sending FREE...");
      sendFREE(); // lasts 1 send_interval
      
      // Listen for unit number
      startTime = millis();        // time of last packet send
      // TIME 1-3: Listen for Sensor ID
      while(!sensor_connected & (millis() - startTime < send_interval*2)) { // receive for 2*send_interval milliseconds
        packetSize = LoRa.parsePacket();
        if (packetSize) {
          Serial.println("Unit Detected.");
          output_LoRa = getConfirmation();
          sensor_connected = checkConnection(sensor_connected, output_LoRa);
        }
        
        if(!sensor_connected) {
          delay(random(send_interval/10));
        }
      }
  
      // if no answer, take a brief nap
      if(!sensor_connected) {
        Serial.println("No Answer. Sleeping...");
        LoRa.sleep();
        Serial.println();
        delay(random(2*send_interval) + send_interval); // wait 2-3 send_intervals
      }
    }
  
    int trans_position = 0; // position in which to save next incoming record - always starts at beginning of array
    if(sensor_connected) {
      // send unit number for confirmation
      Serial.println("Sending Unit Confirmation...");
      // TIME 3-5: Send Unit ID
      sendID_unit(output_LoRa); // lasts 2 send_interval units
    
      // Connection successful - get data
      bool all_records_received = 0; // have all records been received?
    
      int old_position = nrecords; // set initial value for tracking memory position of transmitted data on local sensor node
      while((trans_position < nrecords) & (!all_records_received) & (n < numtries)) { // loop over all records
        bool signal_received = 0;
        n = 0;
        while(!signal_received & n < numtries) { // get a single record
          catch_done = 0; // variable for catching when all records have been sent
          Serial.print("Getting Position: ");
          Serial.println(trans_position);
          Serial.println(millis());
          
          packetSize = 0;
          // TIME 0: Receive Data
          // parse data
          startTime = millis();        // time of last packet send
          while((millis() - startTime) < (2*send_interval)) { // receive for 2*send_interval milliseconds
            if(catch_done == 0) { // only run if transmission has not yet occurred
              packetSize = LoRa.parsePacket();
            }
          
            if (packetSize) {
              if(catch_done == 0) { // only run if transmission has not yet occurred
                catch_done = getData_fun(trans_position, old_position);
                if((catch_done != 0) & (catch_done != 9999)) {
                  if(catch_done==nrecords) {
                    old_position = 0; // indicates that this is the first record sent
                  }
                  else {
                    old_position = catch_done; // updated saved position
                  }
                }
              }
              // else wait until two full seconds are elapsed
            }
            delay(random(send_interval/10));
          }
          if(packetSize) {
            if(catch_done == 9999) { // sending is done
              all_records_received = 1;
              break;
            }
            
            // check checksum
            local_checksum_out = checksum_fun(trans_position);
    
            Serial.print("Record number: ");
            Serial.println(old_position);
            //print_received_record(trans_position);
      
            // send confirmation
            if((local_checksum_out == save_checksum_out[trans_position]) | (save_checksum_out[trans_position]==(-1))) {
              // note: -1 is a signal that there has been a sensor-side error
              Serial.println("Checksum succeeded.");
  
              // TIME 2: Send Confirmation
              startTime = millis();        // time of last packet send
              while((millis() - startTime) < (send_interval)) { // send for send_interval milliseconds
                sendSuccess();
                delay(random(send_interval/10));
              }
              signal_received = 1;
              trans_position = (trans_position+1); // TODO: re-add % nrecords to avoid buffer overflow?
            }
            else {
              Serial.print("Checksum failed. ");
              Serial.print(local_checksum_out);
              Serial.print(" local vs. ");
              Serial.print(save_checksum_out[trans_position]);
              Serial.println(" incoming.");
              delay(send_interval); // wait send_interval milliseconds (to make up for lack of transmission)
            }
          }
          n++;
    
          // note on timing: worst case, both sensors are receiving and sending at the same time
          // need a shift of 1 send_interval to fix this on average
          // thus, the average waiting intervals here should be different from
          // those in the sender code
          delay(send_interval/random(10));
        } // end single record loop
        if(catch_done == 9999) { // sending is done
          Serial.println("Finished Receiving All Records.");
          break;
        }
      } // end all records loop
    }
    if((!sensor_connected) < (n >= numtries)) {
      Serial.println("Receiving Failed - ending attempts.");
    }
  
    // Save to SD card
    if(trans_position>0) {
      // some data sucessfully transmitted

      //TODO: remove below if not needed
      //pinMode(LORA_RESET, OUTPUT);  //LORA reset pin declaration as output
      //digitalWrite(LORA_RESET, LOW);  //turn off LORA module
      
      // open the file. note that only one file can be open at a time,
      // so you have to close this one before opening another.
      int unit_number_log = save_unit_number[1];
      myFile = SD.open("logfile_"+(String)unit_number_log+".csv", FILE_WRITE);
    
      // if the file opened okay, write to it:
      if (myFile) {
        Serial.print("Writing file...");
    
        // Create combined string line for data:
        for(int j = 0; j < nrecords; j++) {
          for(i = 0; i < air_probe_number; i++) {
            myFile.print(save_air_temp[j][i]);
            myFile.print("; ");
          }
          for(i = 0; i < air_probe_number; i++) {
            myFile.print(save_air_humid[j][i]);
            myFile.print("; ");
          }
          for(i = 0; i < soil_temp_probe_number; i++) {
            myFile.print(save_soil_temp[j][i]);
            myFile.print("; ");
          }
          for(i = 0; i < soil_mosit_probe_number; i++) {
            myFile.print(save_soil_moist[j][i]);
            myFile.print("; ");
          }
          myFile.print(save_battery[j]);
          myFile.print("; ");
          for(i = 0; i < 4; i++) {
            myFile.print(save_time[j][i]);
            myFile.print("; ");
          }
          myFile.print(save_rtc_temp[j]);
          myFile.print("; ");
          myFile.print(save_unit_number[j]);
          myFile.print("; ");
          myFile.print(save_checksum_out[j]);
          myFile.println();
        }
        
        myFile.close();
        Serial.println("Saving file done.");
      } else {
        // if the file didn't open, print an error:
        Serial.println("Error opening file.");
      }
    }
  
    Serial.println("End of transmission task.");
    Serial.println();
    LoRa.sleep();
    delay(random(1500, 2500)); // wait 1.5-2.5 seconds
  }

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

  // Get current hour
  digitalWrite(A2, HIGH);
  delay(5000);
  DateTime now = rtc.now();
  digitalWrite(A2, LOW);
  tmp_time = now.hour();
}














void sendFREE() {
  long startTime = millis();        // time of last packet send
  while(millis() - startTime < send_interval) { // send for send_interval milliseconds
    LoRa.beginPacket();
    LoRa.print("FREE"+(String)node_number);
    LoRa.endPacket();
    // TODO: Change to 10?
    delay(random(send_interval/100));
  }
}

String getConfirmation() {
  String output_LoRa = "";
  while (LoRa.available()) {
    // read packet
    output_LoRa = output_LoRa+(char)LoRa.read();
  }
  return(output_LoRa);
}

bool checkConnection(bool sensor_connected, String output_LoRa) {
  if((output_LoRa.substring(0,9) =="SensorID:") & (output_LoRa.substring(9,10) ==String(node_number))) {
    Serial.println(output_LoRa);
    sensor_connected = 1;
  }
  return(sensor_connected);
}

void sendID_unit(String output_LoRa) {
  long startTime = millis();        // time of last packet send
  while(millis() - startTime < (2*send_interval)) {
    LoRa.beginPacket();
    LoRa.print(output_LoRa);
    LoRa.endPacket();
    delay(send_interval/random(10));
  }
  Serial.print(output_LoRa);
  Serial.println(" Sent.");
}

int getData_fun(int trans_position, int old_position) {
  unsigned int i;
  unsigned int j;
  uint8_t buf[4];
  int catch_done = 1; // variable for signalling that data has all been sent
  int new_position;
  
  while (LoRa.available()) {
    // trans_position
    for(j = 0; j < 2; j++) {
        buf[j] = LoRa.read();
    }
    new_position = buf[0] | (buf[1] << 8);

    Serial.print("New position from sensor: ");
    Serial.println(new_position);

    if(new_position == 9999) { // sending is done
      catch_done = 9999;
      break;
    }
    catch_done = new_position; // save new position
    
    // air temp
    for(i = 0; i < air_probe_number; i++) {
      for(j = 0; j < 2; j++) {
        buf[j] = LoRa.read();
      }
      // check whether sending is done;
      save_air_temp[trans_position][i] = buf[0] | (buf[1] << 8);
    }
    // air humid
    for(i = 0; i < air_probe_number; i++) {
      for(j = 0; j < 2; j++) {
        buf[j] = LoRa.read();
      }
      save_air_humid[trans_position][i] = buf[0] | (buf[1] << 8);
    }
    // soil temp
    for(i = 0; i < soil_temp_probe_number; i++) {
      for(j = 0; j < 2; j++) {
        buf[j] = LoRa.read();
      }
      save_soil_temp[trans_position][i] = buf[0] | (buf[1] << 8);
    }
    // soil moist
    for(i = 0; i < soil_mosit_probe_number; i++) {
      for(j = 0; j < 2; j++) {
        buf[j] = LoRa.read();
      }
      save_soil_moist[trans_position][i] = buf[0] | (buf[1] << 8);
    }
    // battery
    for(j = 0; j < 2; j++) {
        buf[j] = LoRa.read();
    }
    save_battery[trans_position] = buf[0] | (buf[1] << 8);
    // time
    for(i = 0; i < 4; i++) {
      for(j = 0; j < 2; j++) {
        buf[j] = LoRa.read();
      }
      save_time[trans_position][i] = buf[0] | (buf[1] << 8);
    }
    // RTC
    for(j = 0; j < 2; j++) {
        buf[j] = LoRa.read();
    }
    save_rtc_temp[trans_position] = buf[0] | (buf[1] << 8);
    // unit
    for(j = 0; j < 2; j++) {
        buf[j] = LoRa.read();
    }
    save_unit_number[trans_position] = buf[0] | (buf[1] << 8);
    // checksum
    for(j = 0; j < 4; j++) {
        buf[j] = LoRa.read();
    }
    save_checksum_out[trans_position] = buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
  }

  return(catch_done);
}

long checksum_fun(int trans_position) {
  unsigned int i;
  long checksum = 0;
  for(i = 0; i < air_probe_number; i++) {
    checksum = checksum + save_air_temp[trans_position][i];
    checksum = checksum + save_air_humid[trans_position][i];
  }
  for(i = 0; i < soil_temp_probe_number; i++) {
    checksum = checksum + save_soil_temp[trans_position][i];
  }
  for(i = 0; i < soil_mosit_probe_number; i++) {
    checksum = checksum + save_soil_moist[trans_position][i];
  }
  checksum = checksum + save_battery[trans_position];
  
  checksum = checksum + save_time[trans_position][0];
  checksum = checksum + save_time[trans_position][1];
  checksum = checksum + save_time[trans_position][2];
  checksum = checksum + save_time[trans_position][3];
  checksum = checksum + save_rtc_temp[trans_position];
  checksum = checksum + save_unit_number[trans_position];

  return(checksum);
}

void sendSuccess() {
  LoRa.beginPacket();
  LoRa.print("Success"+(String)node_number);
  LoRa.endPacket();
}


void print_received_record(long trans_position) {
  unsigned int i;
  Serial.println("Air Temp: ");
  for(i = 0; i < air_probe_number; i++) {
    Serial.println(save_air_temp[trans_position][i]);
  }
  Serial.println("Air Humid: ");
  for(i = 0; i < air_probe_number; i++) {
    Serial.println(save_air_humid[trans_position][i]);
  }
  Serial.println("Soil Temp: ");
  for(i = 0; i < soil_temp_probe_number; i++) {
    Serial.println(save_soil_temp[trans_position][i]);
  }
  Serial.println("Soil Moist: ");
  for(i = 0; i < soil_mosit_probe_number; i++) {
    Serial.println(save_soil_moist[trans_position][i]);
  }
  Serial.print("Battery: ");
  Serial.println(save_battery[trans_position]);
  Serial.println("Time: ");
  for(i = 0; i < 4; i++) {
    Serial.println(save_time[trans_position][i]);
  }
  Serial.print("RTC Temp: ");
  Serial.println(save_rtc_temp[trans_position]);
  Serial.print("Unit: ");
  Serial.println(save_unit_number[trans_position]);
  Serial.print("Check Sum Local: ");
  Serial.println(local_checksum_out);
  Serial.print("Check Sum Incoming: ");
  Serial.println(save_checksum_out[trans_position]);
}
