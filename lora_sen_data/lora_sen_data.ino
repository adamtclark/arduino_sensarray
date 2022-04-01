#include <SPI.h>
#include <LoRa.h>

const int node_number = 1; // unique ID for the node for this unit
const int unit_number = 1; // unique ID for this unit (<=999)
const int scaling_factor = 1E3; // scaling factor for separating unit and node IDs
String sensorID = "SensorID:" + String(node_number*scaling_factor+unit_number);

const int freq = 868E6;
const int send_interval = 400;
const int numtries = 50; // number of tries before taking a break

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
  delay(2000);
  
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
      
        delay(send_interval); // wait for send_interval milliseconds
      
        sendID();
        
        node_connected = checkID();
      }
    }
    delay(random(send_interval/10));

    n++;
    if(n == numtries) {
      LoRa.sleep();
      Serial.println("Sleeping...");
      Serial.println();
      delay(random(2*send_interval) + send_interval); // wait 2-3 send_intervals
      Serial.println("Waiting for FREE...");
    }
    n = n % numtries;
  }

  Serial.println("Done with task.");
  Serial.println();
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
  while(!node_connected) {
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
  }

  Serial.println("Checking Unit ID done.");
  return(node_connected);
}
