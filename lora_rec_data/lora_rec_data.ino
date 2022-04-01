#include <LoRa.h>
#include <SPI.h>

const long freq = 868E6; // for Europe - set to 915E6 for North America
const int SF = 9;
const int node_number = 1; // unique ID for the node for this unit
const int unit_number = 1; // unique ID for this unit (<=999)
const int scaling_factor = 1E3; // scaling factor for separating unit and node IDs
int sensorID = node_number*scaling_factor+unit_number;

const int send_interval = 400;

void setup() {
  Serial.begin(9600);
  //while (!Serial);
  
  Serial.println("LoRa Receiver");

  if (!LoRa.begin(freq)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
  
  //LoRa.setSpreadingFactor(SF);
}

void loop() {
  //delay(5000);

  bool sensor_connected = 0; // no sensor connected yet
  long startTime;
  String output_LoRa;
  
  // Broadcast that the node is free to recieve
  while(!sensor_connected) {
    Serial.println("Sending FREE...");
    Serial.println(millis());
    startTime = millis();        // time of last packet send
    while(millis() - startTime < send_interval) { // send for send_interval milliseconds
      LoRa.beginPacket();
      LoRa.print("FREE");
      LoRa.endPacket();
      delay(random(send_interval/10));
    }
    
    // Listen for unit number
    Serial.println("Listening for Unit...");
    Serial.println(millis());
    startTime = millis();        // time of last packet send
    while(!sensor_connected & (millis() - startTime < send_interval*2)) { // receive for 2*send_interval milliseconds
      int packetSize = LoRa.parsePacket();
      if (packetSize) {
        Serial.println("Unit Detected.");
        Serial.println(millis());
        output_LoRa = "";
        while (LoRa.available()) {
          // read packet
          output_LoRa = output_LoRa+(char)LoRa.read();
        }
        if((output_LoRa.substring(0,9) =="SensorID:") & (output_LoRa.substring(9,10) =="1")) {
          Serial.println(output_LoRa);
          sensor_connected = 0;
        }
        sensor_connected = 1;
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
  Serial.println("Sending Unit Confirmation...");
  Serial.println(millis());
  startTime = millis();        // time of last packet send
  while(millis() - startTime < (send_interval*5)) {
    LoRa.beginPacket();
    LoRa.print(output_LoRa);
    LoRa.endPacket();
    delay(send_interval/random(100));
  }
  Serial.print(output_LoRa);
  Serial.println(" Sent.");
  Serial.println(millis());

  Serial.println("Done with task.");
  Serial.println();
  while(1);
  //Serial.println("Sending...");
  //Serial.print("SensorID:");
  //Serial.println(sensorID);
  
  //LoRa.sleep(); // start in sleep mode
  //delay(500);

  // send packet
  //LoRa.beginPacket();
  //LoRa.print("SensorID:");
  //LoRa.print(sensorID);
  //LoRa.endPacket();

  //Serial.println("Sleeping for 5 sec...");
  //LoRa.sleep();
  //delay(5000);
  //LoRa.sleep(); // start in sleep mode
  //}
}
