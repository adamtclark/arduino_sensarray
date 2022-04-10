const byte PIN_FLASH_CS = 32; // Change this to match the Chip Select pin on your board
#include <SPI.h>
#include <SparkFun_SPI_SerialFlash.h>
SFE_SPI_FLASH myFlash;
const int unit_number = 1; // unique ID for this unit (<=999)
int separator = 9999;
bool firstrun = 1; // fake trigger for example dataset

// fake data
unsigned const int air_probe_number = 2;
unsigned const int soil_temp_probe_number = 2;
unsigned const int soil_mosit_probe_number = 2;

unsigned int write_position = 9; // marker for saved status in RAM
unsigned int save_position = 0; // marker for saved status
long flash_position = 0; // marker for saved status on flash chip
unsigned const int num_records = 10; // number of fake records
long flash_size = 2000000;

int data_air_temp[num_records][air_probe_number];
int data_air_humid[num_records][air_probe_number];
int data_soil_temp[num_records][soil_temp_probe_number];
int data_soil_moist[num_records][soil_mosit_probe_number];
int data_battery[num_records];
int data_time[num_records][4];
int data_rtc_temp[num_records];

void setup() {
  int i;
  int j;
  
  Serial.begin(9600);
  while (!Serial) ;
  delay(100);
  Serial.println("Begin. Making fake data.");

  // make fake data
  for(j = 0; j < num_records; j++) {
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
  }
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


    Serial.println("Saving Data.");
    Serial.print("Write Position: ");
    Serial.println(write_position);
    // save data
    while(save_position<=write_position) {
      Serial.print("Save position:");
      Serial.println(save_position);
      j = save_position;
      
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

      flash_position = flash_position%flash_size; // just in case to avoid buffer overflow
      save_position++;
    }
    Serial.println("Saving Done.");
    Serial.print("Flash Position: ");
    Serial.println(flash_position);
  }
  delay(5000);


  // now check LoRa
}
