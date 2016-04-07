/*
  TeensyTransfer Demo
  
  To make a USB RawHID device, use the Tools > USB Type menu	
*/


#include <TeensyTransfer.h>

const int FlashChipSelect = 6;

void setup() {
  //uncomment these if using Teensy audio shield
  //SPI.setSCK(14);  // Audio shield has SCK on pin 14
  //SPI.setMOSI(7);  // Audio shield has MOSI on pin 7

 //remove unneeded / not connected devices here:
  eeprom_initialize();
 // SerialFlash.begin(FlashChipSelect);
 // ParallelFlash.begin();

}


void loop() {
	ttransfer.transfer();
}
