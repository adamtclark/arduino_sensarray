#include <LoRa.h>
#include <SPI.h>

const long freq = 868E6; // for Europe - set to 915E6 for North America
const int SF = 9;
const int node_number = 1; // unique ID for the node for this unit
const int unit_number = 1; // unique ID for this unit (<=999)
const int scaling_factor = 1E3; // scaling factor for separating unit and node IDs
int sensorID = node_number*scaling_factor+unit_number;

const int send_interval = 500;

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

  Serial.println("Done with task.");
  Serial.println();
  LoRa.sleep();
  while(1);
}


void sendFREE() {
  long startTime = millis();        // time of last packet send
  while(millis() - startTime < send_interval) { // send for send_interval milliseconds
    LoRa.beginPacket();
    LoRa.print("FREE");
    LoRa.endPacket();
    delay(random(send_interval/10));
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
    delay(send_interval/random(100));
  }
  Serial.print(output_LoRa);
  Serial.print(" Sent. ms ");
  Serial.println(millis());
}
