/*
  SD card read/write

  This example shows how to read and write data to and from an SD card file
  The circuit:
   SD card attached to SPI bus as follows:
 ** MOSI - pin 11
 ** MISO - pin 12
 ** CLK - pin 13
 ** CS - pin 4 (for MKRZero SD: SDCARD_SS_PIN)

  created   Nov 2010
  by David A. Mellis
  modified 9 Apr 2012
  by Tom Igoe

  This example code is in the public domain.

*/

#include <SPI.h>
#include <SD.h>

File myFile;
const int nrecords = 100; // number records

void setup() {
  int i;
  int j;
  
  pinMode(LORA_RESET, OUTPUT);  //LORA reset pin declaration as output
  digitalWrite(LORA_RESET, LOW);  //turn off LORA module
  Serial.begin(9600);
  while (!Serial) ; //wait for serial monitor to connect
  delay(500);

  Serial.print("Initializing SD card...");
  if (!SD.begin(32)) {
    Serial.println("initialization failed!");
  }
  else {
    Serial.println("initialization done.");
  
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
          myFile.print("; ")
        }
        for(i = 0; i < air_probe_number; i++) {
          myFile.print(save_air_humid[j][i]);
          myFile.print("; ")
        }
        for(i = 0; i < soil_temp_probe_number; i++) {
          myFile.print(save_soil_temp[j][i]);
          myFile.print("; ")
        }
        for(i = 0; i < soil_mosit_probe_number; i++) {
          myFile.print(save_soil_moist[j][i]);
          myFile.print("; ")
        }
        myFile.print(save_battery[j]);
        myFile.print("; ")
        for(i = 0; i < 4; i++) {
          myFile.print(save_time[j][i]);
          myFile.print("; ")
        }
        myFile.print(save_rtc_temp[j]);
        myFile.print("; ")
        myFile.print(save_unit_number[j]);
        myFile.print("; ")
        myFile.print(save_checksum_out[j]);
        myFile.println()
      }
      
      myFile.close();
      Serial.println("Saving file done.");
    } else {
      // if the file didn't open, print an error:
      Serial.println("Error opening file.");
    }
  }



  delay(500);
}

void loop() {
  // nothing happens after setup
}
