#include <SPI.h>
#include <LoRa.h>
unsigned int i;
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

unsigned const int trans_position = 0; // dummy for tracking position in final script
unsigned const int num_records = 1; // number of fake records

int data_air_temp[num_records][air_probe_number];
int data_air_humid[num_records][air_probe_number];
int data_soil_temp[num_records][soil_temp_probe_number];
int data_soil_moist[num_records][soil_mosit_probe_number];
int data_battery[num_records];
int data_time[num_records][4];
int data_rtc_temp[num_records];




void setup() {
  Serial.begin(9600);
  while (!Serial);

  Serial.println("LoRa Sensor");

  if (!LoRa.begin(freq)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
}

void loop() {
  // make fake data
  for(i = 0; i < air_probe_number; i++) {
    data_air_temp[trans_position][i] = random(2000, 3000);
    data_air_humid[trans_position][i] = random(1400, 2000);
  }
  for(i = 0; i < soil_temp_probe_number; i++) {
    data_soil_temp[trans_position][i] = random(2000, 3000);
  }
  for(i = 0; i < soil_mosit_probe_number; i++) {
    data_soil_moist[trans_position][i] = random(400, 1200);
  }
  data_battery[trans_position] = random(100, 300);
  
  data_time[trans_position][0] = random(1, 12);
  data_time[trans_position][1] = random(1, 31);
  data_time[trans_position][2] = random(0, 23);
  data_time[trans_position][3] = random(0, 59);
  data_rtc_temp[trans_position] = random(2000, 3000);


  long tmp = checksum_fun(trans_position);
  Serial.println("Air Temp: ");
  for(i = 0; i < air_probe_number; i++) {
    Serial.println(data_air_temp[trans_position][i]);
  }
  Serial.println("Air Humid: ");
  for(i = 0; i < air_probe_number; i++) {
    Serial.println(data_air_humid[trans_position][i]);
  }
  Serial.println("Soil Temp: ");
  for(i = 0; i < soil_temp_probe_number; i++) {
    Serial.println(data_soil_temp[trans_position][i]);
  }
  Serial.println("Soil Moist: ");
  for(i = 0; i < soil_mosit_probe_number; i++) {
    Serial.println(data_soil_moist[trans_position][i]);
  }
  Serial.print("Battery: ");
  Serial.println(data_battery[trans_position]);
  Serial.println("Time: ");
  for(i = 0; i < 4; i++) {
    Serial.println(data_time[trans_position][i]);
  }
  Serial.print("RTC Temp: ");
  Serial.println(data_rtc_temp[trans_position]);
  Serial.print("Unit: ");
  Serial.println(unit_number);
  Serial.print("Check Sum: ");
  Serial.println(tmp);
  

  //delay(2000);
  // send data
  bool node_connected = 0; // no sensor connected yet
  int n = 0;
  Serial.print("Waiting for FREE... ms ");
  Serial.println(millis());
  while(!node_connected) {
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
    delay(random(send_interval/10));

    n++;
    if((n == numtries) & !node_connected) {
      LoRa.sleep();
      Serial.println("Sleeping...");
      Serial.println();
      delay(random(send_interval) + 2*send_interval); // wait 2-3 send_intervals
      Serial.println("Waiting for FREE...");
    }
    n = n % numtries;
  }

  
  // connection successful - send data
  bool signal_received = 0;
  n = 0;
  while(!signal_received & n < numtries) {
    Serial.print("Sending Data. ms ");
    Serial.println(millis());
    sendData_fun(trans_position);

    Serial.print("Checking Data. ms ");
    Serial.println(millis());
    signal_received = checkData(signal_received);

    if(!signal_received) {
      Serial.print("Sending Failed. ms ");
      Serial.println(millis());
      delay(random(send_interval/10));
      n++;
    }

    if(n >= numtries) {
      Serial.print("Sending Failed - ending attempts. ms ");
      Serial.println(millis());
    }
  }

  
  Serial.println("Done with task.");
  Serial.println();
  LoRa.sleep();
  // TODO: Still need to add in a break-out if no connection is made
  while(1);
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
  long startTime = millis();        // time of last packet send
  while(millis() - startTime < send_interval) {
    LoRa.beginPacket();
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
    delay(send_interval/random(10));
  }
  Serial.print("Data Sent. ms ");
  Serial.println(millis());
}

bool checkData(bool signal_received) {
  int m = 0;
  while(!signal_received & (m < numtries)) {
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
    delay(random(send_interval/10));
    m++;
  }
  return(signal_received);
}
