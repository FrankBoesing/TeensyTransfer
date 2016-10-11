/* TeensyTransfer library code is placed under the MIT license
 * Copyright (c) 2016 Frank Bösing
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "TeensyTransfer.h"

#if defined(KINETISK) || defined(KINETISL)

#include "version.h"

TeensyTransfer ttransfer;

const unsigned int version = VERSION;

void TeensyTransfer::transfer(void) {
  uint8_t device, mode;
  int n;
    if (!RawHID.available()) return;
	
	RawHID.recv(buffer, 0);
	
	if ( hid_sendAck() < 0 ) {
		//Serial.printf("timeout\n");
		return;
	}

	mode = buffer[0];
	device = buffer[1];
	
	switch (device) {
	#ifdef _HAVE_SERFLASH
		case 0 : {				
				switch (mode) {
					case 0 : serflash_write();break;
					case 1 : serflash_read();break;
					case 2 : serflash_list();break;
					case 3 : serflash_erasefile();break;
					case 4 : serflash_erasedevice();break;
					case 9 : serflash_info();break;
					case 99: serflash_ready();break;
					default: return;
				};
				break;
				}
	#endif

	#ifdef _HAVE_PARFLASH
		case 2 : {			
				switch (mode) {
					case 0 : parflash_write();break;
					case 1 : parflash_read();break;
					case 2 : parflash_list();break;
					case 3 : parflash_erasefile();break;
					case 4 : parflash_erasedevice();break;
					case 9 : parflash_info();break;
					case 99: parflash_ready();break;
					default: return;
				};
				break;
				}
	#endif

	#ifdef _HAVE_EEPROM
		case 253 : {								
				switch (mode) {
					case 0 : eeprom_write();break;
					case 1 : eeprom_read();break;
					default: return;
				}
				break;
				};
	#endif

	#ifdef _HAVE_TEENSY
		case 254 : if (mode==9) teensy_info();
	#endif

		default: return;
		}
}

/********************************************************************************
 common
********************************************************************************/
int TeensyTransfer::hid_sendAck(void) {
	return RawHID.send(buffer, 100);
}

int TeensyTransfer::hid_checkAck(void) {
  char buf2[sizeof(buffer)];
  int n;
	n = RawHID.recv(buffer, 100);
	if (n < 0) return n;
	return memcmp(buffer, buf2, sizeof(buffer));
}


int TeensyTransfer::hid_sendWithAck(void) {
  char buf2[sizeof(buffer)];
  int n;
	n = RawHID.send(buffer, 100);
	if (n < 1) return -1;
	n = RawHID.recv(buf2, 100);
	if (n < 1) return -1;
	n =  memcmp(buffer, buf2, sizeof(buffer));
	if (n) return -1;
	return 0;
}

void TeensyTransfer::val32_buf(const uint32_t val,const uint32_t bufidx)
{
	buffer[bufidx] = val >> 24;
	buffer[bufidx+1] = val >> 16;
	buffer[bufidx+2] = val >> 8;
	buffer[bufidx+3] = val;
}

uint32_t TeensyTransfer::buf_val32(const uint32_t bufidx)
{
	return ( ((uint32_t)buffer[bufidx]<<24) | 
			((uint32_t)buffer[bufidx+1]<<16) | 
			((uint32_t)buffer[bufidx+2]<<8) | 
			((uint32_t)buffer[bufidx+3])
			);
}

/********************************************************************************
 Serial Flash
********************************************************************************/
#ifdef _HAVE_SERFLASH

void TeensyTransfer::serflash_write(void) {
  int  n, r;
  size_t sz, pos;
  char filename[64];

	sz = buf_val32(2);
	n = RawHID.recv(buffer, 500);
	if (n < 0) {
		//Serial.printf("timeout\n");
		return;
	}

	strcpy( filename, (char*) &buffer[0]);
	//Serial.printf("Filename:%s\n", &filename[0]);
	if (SerialFlash.exists(filename)) {
		//Serial.printf("File exists. Deleting.\n");
		SerialFlash.remove(filename);
	}

	hid_sendAck();

	r = SerialFlash.create(filename, sz);
	if (!r) {
		//Serial.printf("unable to create file.\n");
		return;
	}
	SerialFlashFile ff = SerialFlash.open(filename);
	if (!ff) {
		//Serial.printf("error opening freshly created file!\n");
		return;
	}
	//Serial.print("Flashing.\n");

	pos = 0;
	while (pos < sz) {
		n = RawHID.recv(buffer, 500);
		if (n < 0) {
//     	  Serial.printf("timeout\n");
			return;
		}
		ff.write(buffer, sizeof(buffer));
		pos += sizeof(buffer);
	}
	ff.close();
	hid_sendAck();
	//Serial.println("ok.");
}




void TeensyTransfer::serflash_read(void) {
  int n,r;
  unsigned int sz;
  char filename[64];

	//Serial.print("Read");
	n = RawHID.recv(buffer, 500);
	if (n >= 0) n = hid_sendAck();
	if (n < 0) {
//      Serial.printf("timeout\n");
	  return;
	}

	strcpy( filename, (char*) &buffer[0]);
	//Serial.println(filename);

	if (SerialFlash.exists(filename))
		buffer[0] = 1;
	else
		buffer[0] = 2;

	SerialFlashFile ff = SerialFlash.open(filename);

	sz = ff.size();
	val32_buf(sz,1);
	//Serial.printf("Size:%d",sz);

	n = hid_sendWithAck();
	if (n) {
//      Serial.println("Error");
		return;
	}

	//Send file to PC
	do {
		r = ff.read( buffer, sizeof(buffer) );
		if (r) {
			n = hid_sendWithAck();
			if (n) {
//          	Serial.println("Error");
			return;
			}
		}
	} while (r > 0);
	ff.close();
}

void TeensyTransfer::serflash_list(void) {
  int n;
  uint32_t sz;
//    Serial.println("List");
	SerialFlash.opendir();
	char buf2[64];
	while (1) {
		if (SerialFlash.readdir(buf2, sizeof(buf2), sz)) {

			//Send Filesize
			buffer[0] = 1;
			val32_buf(sz,1);

			n = hid_sendWithAck();
			if (n) {
	//          Serial.println("Error");
				return;
			}

			//Send Filename
			memcpy(buffer, buf2, sizeof(buffer));
			n = hid_sendWithAck();
			if (n) {
				//Serial.println("Error");
				return;
			}
	  }
	  else break;
	}
	//Send Fee Space
	/*
	  sz = SerialFlash.available();
	  buffer[0] = 2;
	  val32_buf(sz,1);
	  n = hid_sendWithAck();
	  if (n) {
	  Serial.println("Error");
	  return;
	  }
	*/
	//Send EOF
	buffer[0] = 0;
	n = hid_sendWithAck();
	if (n) {
//      Serial.println("Error");
		return;
	}
	//SerialFlash.closedir();
}

void TeensyTransfer::serflash_erasefile(void) {
  int n;
  char filename[64];
//    Serial.println("erase");
	n = RawHID.recv(buffer, 100);
	if (n < 0) {
//      Serial.printf("timeout\r\n");
		return;
	}

	strcpy( filename, (char*) &buffer[0]);
//	Serial.printf("Filename:%s\r\n", &filename[0]);
	if (SerialFlash.exists(filename)) {
//  	Serial.printf("File exists. Deleting.\r\n");
		SerialFlash.remove(filename);
		buffer[0] = 1;
	} else buffer[0] = 0;

	RawHID.send(buffer, 100);
	return;
}

void TeensyTransfer::serflash_erasedevice() {
	SerialFlash.eraseAll();	
}

void TeensyTransfer::serflash_ready() {
	buffer[0] = !SerialFlash.ready();
	buffer[1] = 17; //magic
	RawHID.send(buffer, 100);
}

void TeensyTransfer::serflash_info(void) {
uint32_t sz;	
	buffer[0]=1;
	SerialFlash.readID(&buffer[8]);
	sz = SerialFlash.capacity(&buffer[8]);
	val32_buf(sz,1);

	SerialFlash.readSerialNumber(&buffer[16]);
	RawHID.send(buffer, 100);
}

#endif //#ifdef _HAVE_SERFLASH

/********************************************************************************
 Parallel Flash
********************************************************************************/
#ifdef _HAVE_PARFLASH

void TeensyTransfer::parflash_write(void) {
  int  n, r;
  size_t sz, pos;
  char filename[64];

	sz = buf_val32(2);

	n = RawHID.recv(buffer, 500);
	if (n < 0) {
		//Serial.printf("timeout\n");
		return;
	}

	strcpy( filename, (char*) &buffer[0]);
	//Serial.printf("Filename:%s\n", &filename[0]);
	if (ParallelFlash.exists(filename)) {
		//Serial.printf("File exists. Deleting.\n");
		ParallelFlash.remove(filename);
	}

	hid_sendAck();

	r = ParallelFlash.create(filename, sz);
	if (!r) {
		//Serial.printf("unable to create file.\n");
		return;
	}
	ParallelFlashFile ff = ParallelFlash.open(filename);
	if (!ff) {
		//Serial.printf("error opening freshly created file!\n");
		return;
	}
	//Serial.print("Flashing.\n");

	pos = 0;
	while (pos < sz) {
		n = RawHID.recv(buffer, 500);
		if (n < 0) {
//     	  	Serial.printf("timeout\n");
			return;
		}
		ff.write(buffer, sizeof(buffer));
		pos += sizeof(buffer);
	}
	ff.close();
	hid_sendAck();
	//Serial.println("ok.");
}




void TeensyTransfer::parflash_read(void) {
  int n,r;
  unsigned int sz;
  char filename[64];

	//Serial.print("Read");
	n = RawHID.recv(buffer, 500);
	if (n >= 0) n = hid_sendAck();
	if (n < 0) {
//      Serial.printf("timeout\n");
		return;
	}

	strcpy( filename, (char*) &buffer[0]);
	//Serial.println(filename);

	if (ParallelFlash.exists(filename))
		buffer[0] = 1;
	else
		buffer[0] = 2;

	ParallelFlashFile ff = ParallelFlash.open(filename);

	sz = ff.size();
	val32_buf(sz,1);
	//Serial.printf("Size:%d",sz);

	n = hid_sendWithAck();
	if (n) {
//      Serial.println("Error");
		return;
	}

	//Send file to PC
	do {
		r = ff.read( buffer, sizeof(buffer) );
		if (r) {
			n = hid_sendWithAck();
			if (n) {
//          	Serial.println("Error");
				return;
			}
		}
	} while (r > 0);
	ff.close();
}

void TeensyTransfer::parflash_list(void) {
  int n;
  uint32_t sz;
//    Serial.println("List");
	ParallelFlash.opendir();
	char buf2[64];
	while (1) {
		if (ParallelFlash.readdir(buf2, sizeof(buf2), sz)) {

			//Send Filesize
			buffer[0] = 1;
			val32_buf(sz,1);
			n = hid_sendWithAck();
			if (n) {
	//          Serial.println("Error");
				return;
			}

			//Send Filename
			memcpy(buffer, buf2, sizeof(buffer));
			n = hid_sendWithAck();
			if (n) {
				//Serial.println("Error");
				return;
			}
	  }
	  else break;
	}
	//Send Fee Space
	/*
	  sz = ParallelFlash.available();
	  buffer[0] = 2;
	  buffer[1] = (sz >> 24) & 0xff;
	  buffer[2] = (sz >> 16) & 0xff;
	  buffer[3] = (sz >> 8) & 0xff;
	  buffer[4] = sz & 0xff;

	  n = hid_sendWithAck();
	  if (n) {
	  Serial.println("Error");
	  return;
	  }
	*/
	//Send EOF
	buffer[0] = 0;
	n = hid_sendWithAck();
	if (n) {
//      Serial.println("Error");
		return;
	}
	//ParallelFlash.closedir();
}

void TeensyTransfer::parflash_erasefile(void) {
  int n;
  char filename[64];
//    Serial.println("erase");
	n = RawHID.recv(buffer, 100);
	if (n < 0) {
//      Serial.printf("timeout\r\n");
		return;
	}

	strcpy( filename, (char*) &buffer[0]);
//	Serial.printf("Filename:%s\r\n", &filename[0]);
	if (ParallelFlash.exists(filename)) {
//  	Serial.printf("File exists. Deleting.\r\n");
		ParallelFlash.remove(filename);
		buffer[0] = 1;
	} else buffer[0] = 0;

	RawHID.send(buffer, 100);
	return;
}

void TeensyTransfer::parflash_erasedevice() {
	ParallelFlash.eraseAll();	
}

void TeensyTransfer::parflash_ready() {
	buffer[0] = !ParallelFlash.ready();
	buffer[1] = 17; //magic
	RawHID.send(buffer, 100);
}

void TeensyTransfer::parflash_info(void) {
  uint32_t sz;

	buffer[0]=1;
	ParallelFlash.readID(&buffer[8]);
	sz = ParallelFlash.capacity(&buffer[8]);
	val32_buf(sz,1);

	ParallelFlash.readSerialNumber(&buffer[16]);
	RawHID.send(buffer, 100);
}
#endif //#ifdef _HAVE_PARFLASH

/********************************************************************************
 Teensy
********************************************************************************/
#ifdef _HAVE_TEENSY

void TeensyTransfer::teensy_info(void) {		
		//Teensy Model
		#if defined(__MK20DX128__)
		buffer[0]=1;
		#elif defined(__MK20DX256__)
		buffer[0]=2;
		#elif defined(__MKL26Z64__)
		buffer[0]=3;
		#elif defined(__MK64FX512__)
		buffer[0]=4;
		#elif defined(__MK66FX1M0__)
		buffer[0]=5;
		#else
		buffer[0]=0;
		#endif

		//Compiled-in devices:
		unsigned int d = 1 ; //Teensy
		#ifdef _HAVE_EEPROM
		d |= 2;
		#endif
		#ifdef _HAVE_SERFLASH
		d |= 4;
		#endif
		#ifdef _HAVE_PARFLASH
		d |= 8;
		#endif
		val32_buf(d,6); 		
		val32_buf(version,10); 
		
		val32_buf((E2END) + 1,1);		
		val32_buf(F_CPU,16);
		val32_buf(F_PLL,20);
		val32_buf(F_BUS,24);
		val32_buf(F_MEM,28);
		
		__disable_irq();
		
#if defined(HAS_KINETIS_FLASH_FTFA) || defined(HAS_KINETIS_FLASH_FTFL)
		
		//MAC
		FTFL_FCCOB0 = 0x41;             // Selects the READONCE command
        FTFL_FCCOB1 = 0x0e;             // read the given word of read once area
                                        // -- this is one half of the mac addr.
        FTFL_FSTAT = FTFL_FSTAT_CCIF;   // Launch command sequence
        while(!(FTFL_FSTAT & FTFL_FSTAT_CCIF)) {}
        buffer[58] = FTFL_FCCOB5;       // collect only the top three bytes,
        buffer[59] = FTFL_FCCOB6;       // in the right orientation (big endian).
        buffer[60] = FTFL_FCCOB7;       // Skip FTFL_FCCOB4 as it's always 0.
		//FTFL_FCCOB0 = 0x41;             // Selects the READONCE command
        FTFL_FCCOB1 = 0x0f;             // read the given word of read once area
                                        // -- this is one half of the mac addr.
        FTFL_FSTAT = FTFL_FSTAT_CCIF;   // Launch command sequence
        while(!(FTFL_FSTAT & FTFL_FSTAT_CCIF)) {}
        buffer[61] = FTFL_FCCOB5;       // collect only the top three bytes,
        buffer[62] = FTFL_FCCOB6;       // in the right orientation (big endian).
        buffer[63] = FTFL_FCCOB7;       // Skip FTFL_FCCOB4 as it's always 0.

#elif defined(HAS_KINETIS_FLASH_FTFE) //MK64/MK66

// Does not work in HSRUN mode :
#if F_CPU>120000000
#error LOWER CPU SPEED TO 120MHz!!! ...or remove this line and know that the printed Serial and MAC will not be correct :)
#endif

		
		buffer[58] = 0x04;
		buffer[59] = 0xE9; 
		buffer[60] = 0xE5;

		FTFL_FSTAT = FTFL_FSTAT_RDCOLERR | FTFL_FSTAT_ACCERR | FTFL_FSTAT_FPVIOL;
		*(uint32_t *)&FTFL_FCCOB3 = 0x41070000;
		FTFL_FSTAT = FTFL_FSTAT_CCIF;
		while (!(FTFL_FSTAT & FTFL_FSTAT_CCIF)) ; // wait
		uint32_t num = *(uint32_t *)&FTFL_FCCOBB;
		buffer[61] = num >> 16;
		buffer[62] = num >> 8;
		buffer[63] = num;
#endif
		__enable_irq();
		RawHID.send(buffer, 100);
		
}
#endif

/********************************************************************************
 EEPROM
********************************************************************************/
 #ifdef _HAVE_EEPROM
void TeensyTransfer::eeprom_read(void) {
  int n;
  size_t sz;
  uint8_t *p;

	sz = (E2END) + 1;
	val32_buf(sz, 1);
	//Serial.printf("Size:%d",sz);
	n = hid_sendWithAck();
	if (n) {
//      Serial.println("Error");
	  return;
	}

	p = 0;
	//send EEprom to PC
	do {
		eeprom_read_block(buffer,p, sizeof(buffer));
		n = hid_sendWithAck();
		if (n) {
//      	Serial.println("Error");
			return;
		}
		p += sizeof(buffer);
	} while (p < (uint8_t*)sz);
}

void TeensyTransfer::eeprom_write(void) {
  int n;
  size_t sz;
  uint8_t *p;

	sz = (E2END) + 1;
	val32_buf(sz, 1);
	//Serial.printf("Size:%d",sz);
	n = hid_sendWithAck();
	if (n) {
//      Serial.println("Error");
		return;
	}

	p = 0;
	while (p < (uint8_t*)sz) {
		n = RawHID.recv(buffer, 500);
		if (n < 0) {
//      	Serial.printf("timeout\n");
			return;
		}
		eeprom_write_block(buffer,p, sizeof(buffer));
		p += sizeof(buffer);
	}

	hid_sendAck();
	//Serial.println("ok.");
}
#endif //#ifdef _HAVE_EEPROM

#endif
