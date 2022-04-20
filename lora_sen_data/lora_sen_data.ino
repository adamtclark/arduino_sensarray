#include <SPI.h>
#include <LoRa.h>
unsigned int i;
unsigned int j;
const int node_number = 1; // unique ID for the node for this unit
const int unit_number = 1; // unique ID for this unit (<=999)
const int scaling_factor = 1E3; // scaling factor for separating unit and node IDs
String sensorID = "SensorID:" + String(node_number*scaling_factor+unit_number);

const int freq = 868E6;
const int send_interval = 500;
const int numtries = 50; // number of tries before taking a break


// fake data
unsigned const int air_probe_number = 2;
unsigned const int soil_temp_probe_number = 2;
unsigned const int soil_mosit_probe_number = 2;

unsigned const int nrecords = 10; // number of fake records
int trans_position = nrecords; // last record transmitted - "nrecords" indicates that no data have been sent yet
unsigned int samp_position = nrecords-1; // position in data array

int data_air_temp[nrecords][air_probe_number];
int data_air_humid[nrecords][air_probe_number];
int data_soil_temp[nrecords][soil_temp_probe_number];
int data_soil_moist[nrecords][soil_mosit_probe_number];
int data_battery[nrecords];
int data_time[nrecords][4];
int data_rtc_temp[nrecords];




void setup() {
  Serial.begin(9600);
  //while (!Serial);

  Serial.println("LoRa Sensor");

  if (!LoRa.begin(freq)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
}

void loop() {
  // make fake data
  for(j = 0; j < nrecords; j++) {
    for(i = 0; i < air_probe_number; i++) {
      data_air_temp[j][i] = random(2000, 3000);
      data_air_humid[j][i] = random(1400, 2000);
    }
    for(i = 0; i < soil_temp_probe_number; i++) {
      data_soil_temp[j][i] = random(2000, 3000);
    }
    for(i = 0; i < soil_mosit_probe_number; i++) {
      data_soil_moist[j][i] = random(400, 1200);
    }
    data_battery[j] = random(100, 300);
    
    data_time[j][0] = random(1, 12);
    data_time[j][1] = random(1, 31);
    data_time[j][2] = random(0, 23);
    data_time[j][3] = random(0, 59);
    data_rtc_temp[j] = random(2000, 3000);

    Serial.print("Trans position: ");
    Serial.println(j);
    
    Serial.println("Air Temp: ");
    for(i = 0; i < air_probe_number; i++) {
      Serial.println(data_air_temp[j][i]);
    }
    Serial.println("Air Humid: ");
    for(i = 0; i < air_probe_number; i++) {
      Serial.println(data_air_humid[j][i]);
    }
    Serial.println("Soil Temp: ");
    for(i = 0; i < soil_temp_probe_number; i++) {
      Serial.println(data_soil_temp[j][i]);
    }
    Serial.println("Soil Moist: ");
    for(i = 0; i < soil_mosit_probe_number; i++) {
      Serial.println(data_soil_moist[j][i]);
    }
    Serial.print("Battery: ");
    Serial.println(data_battery[j]);
    Serial.println("Time: ");
    for(i = 0; i < 4; i++) {
      Serial.println(data_time[j][i]);
    }
    Serial.print("RTC Temp: ");
    Serial.println(data_rtc_temp[j]);
    Serial.print("Unit: ");
    Serial.println(unit_number);
    Serial.print("Check Sum: ");
    long tmp = checksum_fun(j);
    Serial.println(tmp);
  }
  

  //delay(2000);
  // send data
  bool node_connected = 0; // no sensor connected yet
  int n = 0;
  long startTime;
  unsigned int tmp_trans_position; // temporary position for sending data

  //TODO: before begin, check whether trans_position == samp_position
  
  Serial.print("Waiting for FREE... ms ");
  Serial.println(millis());
  while(!node_connected & (n < numtries)) {
    int packetSize = LoRa.parsePacket();
    if (packetSize) {
      Serial.print("Signal Detected. ms ");
      Serial.println(millis());
      String output_LoRa = getPacket();
      
      if(output_LoRa == "FREE") {
        Serial.print("Node Detected. ID ");
        Serial.print(output_LoRa);
        Serial.print(" . ms ");
        Serial.println(millis());
      
        delay(random(0.4*send_interval) + 0.8*send_interval); // wait for 0.8 to 1.2 send_interval milliseconds
      
        sendID();
        
        node_connected = checkID();
      }
    }
    else {
      delay(random(send_interval/10));
    }

    n++;
  }

  bool all_signals_received = 0; // has transmission caught up to sampling?
  if(node_connected) {
    // connection successful - send data

    Serial.print("Sample position ");
    Serial.print(samp_position);
    Serial.print(". ms ");
    Serial.println(millis());
    n = 0; // start counter for maximum tries
    
    while(!all_signals_received  & n < numtries) {
      bool signal_received = 0;
      if(trans_position == nrecords) { // no records have been sent yet
        tmp_trans_position = 0;
      }
      else {
        tmp_trans_position = ((trans_position+1) % nrecords); // record to attempt to send
      }
      
      Serial.print("Transmitting record number ");
      Serial.print(tmp_trans_position);
      Serial.print(". ms ");
      Serial.println(millis());
      while(!signal_received & n < numtries) {
        Serial.print("Sending Data. ms ");
        Serial.println(millis());
        
        startTime = millis();        // time of last packet send
        while((millis() - startTime) < (send_interval)) { // send for send_interval milliseconds
          delay(random(send_interval/10));
          sendData_fun(tmp_trans_position);
        }
    
        Serial.print("Checking Data. ms ");
        Serial.println(millis());
        startTime = millis();        // time of last packet send
        while((millis() - startTime) < (2*send_interval)) { // receive for 2*send_interval milliseconds
          if(signal_received == 0) { // only run if signal not yet checked
            delay(random(send_interval/10));
            signal_received = checkData(signal_received);
          }
        }
        delay(random(send_interval/10));
    
        if(!signal_received) {
          Serial.print("Sending Failed. ms ");
          Serial.println(millis());
          n++;
        }
    
        if(n >= numtries) {
          Serial.print("Sending Failed - ending attempts. ms ");
          Serial.println(millis());
        }
      }
      if(signal_received) {
        n = 0; // reset try counter
        trans_position = tmp_trans_position; // increment transfer counter
        if(trans_position == samp_position) {
          all_signals_received = 1;
          Serial.print("All Records Successfully Received: ms ");
          Serial.println(millis());
        }
      }
    }
  }
  else {
    Serial.print("Connection Failed - ending attempts. ms ");
    Serial.println(millis());
  }

  startTime = millis();        // time of last packet send
  while(millis() - startTime < send_interval) { // send Done signal for send_interval milliseconds
    sendDone(); // Send message that sending is done
    delay(random(send_interval/10));
  }

  if(!node_connected | !all_signals_received) {
    // transfer failed - go to sleep and try again
    LoRa.sleep();
    Serial.println("Connection and/or Sending Failed. Sleeping...");
    Serial.println();
    delay(random(send_interval) + 2*send_interval); // wait 2-3 send_intervals
  }

  
  Serial.println("End of task.");
  Serial.println();
  LoRa.sleep();
  delay(random(send_interval) + 2*send_interval); // wait 2-3 send_intervals
  
  // TODO: Still need to add in a break-out if no connection is made
  //while(1);
}












String getPacket() {
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
    delay(send_interval/random(100));
  }
  Serial.print(sensorID);
  Serial.print(" Sent. ms ");
  Serial.println(millis());
}

bool checkID() {
  bool node_connected = 0;
  Serial.print("Checking Unit ID: ms ");
  Serial.println(millis());

  int m = 0;
  while(!node_connected & (m < numtries)) {
    int packetSize = LoRa.parsePacket();
    if (packetSize) {
      // read packet
      String output_LoRa = "";
      while (LoRa.available()) {
        output_LoRa = output_LoRa+(char)LoRa.read();
      }
      Serial.print("Output is: ");
      Serial.print(output_LoRa);
      Serial.print(". ms ");
      Serial.println(millis());
      
      if(sensorID ==output_LoRa) {
        Serial.print("Successfully coupled. Node ");
        Serial.print(output_LoRa);
        Serial.print(" . ms ");
        Serial.println(millis());
        node_connected = 1;
      }
    }
    delay(random(send_interval/10));
    m++;
  }

  Serial.println("Checking Unit ID done.");
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
  LoRa.write((uint8_t *)&(trans_position), 2);
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
  
  Serial.print("Data Sent. ms ");
  Serial.println(millis());
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
    Serial.print(output_LoRa);
    Serial.print(". ms ");
    Serial.println(millis());
    
    if(output_LoRa == "Success") {
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

  Serial.print("Sending Done. ms ");
  Serial.println(millis());
}
