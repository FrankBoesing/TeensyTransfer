/*
  TeensyTransfer Demo
  
  To make a USB RawHID device, use the Tools > USB Type menu	
*/


#include <TeensyTransfer.h>

const int FlashChipSelect = 6;

void setup() 
{

//>>>UNCOMMENT THESE IF USING THE TEENSY AUDIO SHIELD

//  SPI.setSCK(14);  // Audio shield has SCK on pin 14
//  SPI.setMOSI(7);  // Audio shield has MOSI on pin 7


//>>>REMOVE unneeded / not connected devices here:

  eeprom_initialize();
// SerialFlash.begin(FlashChipSelect);
// ParallelFlash.begin();

}


void loop() {
	ttransfer.transfer();
}
