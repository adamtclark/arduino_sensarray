const byte PIN_FLASH_CS = 32; // Change this to match the Chip Select pin on your board
#include <SPI.h>
#include <SparkFun_SPI_SerialFlash.h>
SFE_SPI_FLASH myFlash;
//#include <LoRa.h>
const int unit_number = 1; // unique ID for this unit (<=999)
int separator = 9999;
bool firstrun = 1; // fake trigger for example dataset
long flash_size = 2000000;
long flash_position = 0;

void setup() {
  int i;
  int j;
  
  Serial.begin(9600);
  while (!Serial) ;
  delay(100);
}

void loop() {
  int i; int j;
  long ilong;

  if(firstrun) {
    firstrun = 0;
    // set up flash
    Serial.println("Turning off LoRa.");
    pinMode(LORA_RESET, OUTPUT);  //LORA reset pin declaration as output
    digitalWrite(LORA_RESET, LOW);  //turn off LORA module
    Serial.println("LoRa Off.");
    delay(500);
  
    Serial.print("Beginning Flash. Status: ");
    bool flashstart = myFlash.begin(PIN_FLASH_CS, 2000000, SPI1);
    Serial.println(flashstart);


    // find start position in flash
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


    Serial.println("Reading out values:");
    for(ilong = 0; ilong<flash_position; ilong+=2) {
      int tmpval = myFlash.readByte(ilong) | (myFlash.readByte(ilong+1) << 8);
      if(tmpval != separator) {
        Serial.print(tmpval);
        Serial.print("; ");
      }
      else {
        Serial.println();
      }
    }

    Serial.println("Done.");
  }
  delay(5000);

  /*
  // now check LoRa
  const int freq = 868E6;
  if (!LoRa.begin(freq)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }

  int counter = 0;
  while(1) {
    Serial.print("Sending packet: ");
    Serial.println(counter);
  
    // send packet
    LoRa.beginPacket();
    LoRa.print("hello ");
    LoRa.print(counter);
    LoRa.endPacket();
  
    counter++;
  
    delay(5000);
  }
  */
}
