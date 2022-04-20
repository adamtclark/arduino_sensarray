#include <LoRa.h>
#include <SPI.h>

const long freq = 868E6; // for Europe - set to 915E6 for North America
const int SF = 9;
const int node_number = 1; // unique ID for the node for this unit
const int unit_number = 1; // unique ID for this unit (<=999)
const int scaling_factor = 1E3; // scaling factor for separating unit and node IDs
int sensorID = node_number*scaling_factor+unit_number;

const int send_interval = 500;
const int numtries = 50; // number of tries before taking a break

// save data
unsigned const int nrecords = 10; // number records

unsigned const int air_probe_number = 2;
unsigned const int soil_temp_probe_number = 2;
unsigned const int soil_mosit_probe_number = 2;

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

void setup() {
  Serial.begin(9600);
  while (!Serial);
  
  Serial.println("LoRa Receiver");

  if (!LoRa.begin(freq)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
  
  //LoRa.setSpreadingFactor(SF);
}

void loop() {
  int i;
  unsigned int n = 0;
  //delay(2000);

  long startTime;
  String output_LoRa;
  
  // Broadcast that the node is free to recieve
  bool sensor_connected = 0;
  while(!sensor_connected) {
    Serial.print("Sending FREE... ms ");
    Serial.println(millis());
    sendFREE();
    
    // Listen for unit number
    Serial.print("Listening for Unit... ms ");
    Serial.println(millis());
    startTime = millis();        // time of last packet send
    while(!sensor_connected & (millis() - startTime < send_interval*2)) { // receive for 2*send_interval milliseconds
      int packetSize = LoRa.parsePacket();
      if (packetSize) {
        Serial.print("Unit Detected. ms ");
        Serial.println(millis());
        output_LoRa = getPacket();
        
        sensor_connected = checkConnection(sensor_connected, output_LoRa);
      }
      
      if(!sensor_connected) {
        delay(random(send_interval/10));
      }
    }

    // if no answer, take a brief nap
    if(!sensor_connected) {
      Serial.println("Sleeping...");
      LoRa.sleep();
      Serial.println();
      delay(random(2*send_interval) + send_interval); // wait 2-3 send_intervals
    }
  }

  // send unit number for confirmation
  Serial.print("Sending Unit Confirmation... ms ");
  Serial.println(millis());
  sendID_unit(output_LoRa);

  
  // connection successful - get data
  unsigned int trans_position = 0; // position in which to save next incoming record - always starts at beginning of array
  bool all_records_received = 0; // have all records been received?
  int catch_done = 0; // variable for catching when all records have been sent
  
  while((trans_position < nrecords) & (!all_records_received) & (n < numtries)) {
    bool signal_received = 0;
    n = 0;
    while(!signal_received & n < numtries) {
      Serial.print("Getting Position: ");
      Serial.print(trans_position);
      Serial.print(". ms ");
      Serial.println(millis());
      
      int packetSize = LoRa.parsePacket();
      if (packetSize) {
        // parse data
        catch_done = getData_fun(trans_position);
        if(catch_done == 9999) { // sending is done
          all_records_received = 1;
          break;
        }
        
        // check checksum
        local_checksum_out = checksum_fun(trans_position);
        print_received_record(trans_position);
  
        // send confirmation
        if(local_checksum_out == save_checksum_out[trans_position]) {
          Serial.print("Checksum succeeded. ms ");
          Serial.println(millis());
          
          sendSuccess();
          signal_received = 1;
          trans_position++; // proceed to next record
        }
        else {
          Serial.print("Checksum failed. ");
          Serial.print(local_checksum_out);
          Serial.print(" local vs. ");
          Serial.print(save_checksum_out[trans_position]);
          Serial.print(" incoming. ms ");
          Serial.println(millis());
        }
      }
      n++;
      delay(send_interval/random(10));
    } // end single record loop
    if(catch_done == 9999) { // sending is done
      Serial.print("Finished Receiving All Records. ms ");
      Serial.println(millis());
      break;
    }
  } // end all records loop

  if(n >= numtries) {
    Serial.print("Receiving Failed - ending attempts. ms ");
    Serial.println(millis());
  }
  
  // TODO: Need to add saving data to SD card.

  Serial.println("End of task.");
  Serial.println();
  LoRa.sleep();
  delay(random(2*send_interval) + send_interval); // wait 2-3 send_intervals
  //while(1);
}














void sendFREE() {
  long startTime = millis();        // time of last packet send
  while(millis() - startTime < send_interval) { // send for send_interval milliseconds
    LoRa.beginPacket();
    LoRa.print("FREE");
    LoRa.endPacket();
    delay(random(send_interval/100));
  }
}

String getPacket() {
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
  while(millis() - startTime < (send_interval*2)) {
    LoRa.beginPacket();
    LoRa.print(output_LoRa);
    LoRa.endPacket();
    delay(send_interval/random(10));
  }
  Serial.print(output_LoRa);
  Serial.print(" Sent. ms ");
  Serial.println(millis());
}

int getData_fun(unsigned int trans_position) {
  unsigned int i;
  unsigned int j;
  uint8_t buf[4];
  int catch_done = 0; // variable for signalling that data has all been sent
  long startTime = millis();        // time of last packet send
  while(millis() - startTime < (2*send_interval)) {   
    while (LoRa.available()) {
      // air temp
      for(i = 0; i < air_probe_number; i++) {
        for(j = 0; j < 2; j++) {
          buf[j] = LoRa.read();
        }
        // check whether sending is done;
        if(i == 0) {
          catch_done = buf[0] | (buf[1] << 8);
          if(catch_done == 9999) {
            break;
          }
          else {
            save_air_temp[trans_position][i] = catch_done;
          }
        }
        else {
          save_air_temp[trans_position][i] = buf[0] | (buf[1] << 8);
        }
      }
      if(catch_done == 9999) { // sending is done
        break;
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
    if(catch_done == 9999) { // sending is done
      break;
    }
    delay(send_interval/random(10));
  }
  Serial.print("Data Received. ms ");
  Serial.println(millis());

  return(catch_done);
}

long checksum_fun(unsigned int trans_position) {
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
  long startTime = millis();        // time of last packet send
  while(millis() - startTime < (send_interval*2)) { // send for send_interval milliseconds
    LoRa.beginPacket();
    LoRa.print("Success");
    LoRa.endPacket();
    delay(random(send_interval/10));
  }
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
