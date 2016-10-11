#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <libgen.h>

#if defined(OS_LINUX) || defined(OS_MACOSX)
#include <sys/ioctl.h>
#include <termios.h>
#define DELAY(x) { nanosleep((const struct timespec[]){{0, x * 1000L* 1000L}}, NULL); }
#elif defined(OS_WINDOWS)
#include <conio.h>
#include <fcntl.h>
#include <windows.h>
unsigned int _CRT_fmode = _O_BINARY;
#define DELAY(x) { Sleep( x ); }
#endif

#include "hid.h"
#include "version.h"


#define DOTCNTPACKETS 64 //Print a dot after x packets (64*64 bytes=4 KB)

const int timeout = 400;
const int hid_device = 0;

unsigned char buf[64];


#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

// options (from user via command line args)
int device = 0;
int mode = 0;
int verbose = 0;
const char *fname=NULL;

/***********************************************************************************************************/
void die(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	fprintf(stderr, "teensytransfer: ");
	vfprintf(stderr, format, args);
	fprintf(stderr, "\n");
	exit(1);
}


void usage(const char *err)
{
	if(err != NULL) fprintf(stderr, "%s\n\n", err);
	fprintf(stderr,
		"Compiled " __DATE__ " " __TIME__ "\n"
		"Usage: teensytransfer [-w] [-r] [-l] [-d] [device] <file>\n"
		"\t-w : write (default)\n"
		"\t-r : read\n"
		"\t-d : delete file\n"
		"\t-e : erase device\n"
		"\t-l : list files\n"
		"\t-i : device info\n"
		//"\t-v : Verbose output\n"
		"\n"
		"\t Devices:\n"
		"\tteensy\n"
		"\tserflash (default)\n"
//		"\tparflash\n"
		"\teeprom\n"
//		"\tsdcard\n"
//		"\tdev0..dev7\n"
		"");
	exit(1);
}

void debug(void) __attribute__((__unused__));
void debug(void)
{
	printf("USB:");
	int i;
	for (i=0;i<sizeof(buf);i++) printf("%x ",buf[i]);
	printf("\n");
}

void parse_flag(char *arg)
{
	int i;
	for(i=1; arg[i]; i++) {
		switch(arg[i]) {
			case 'w': mode = 0; break; //write
			case 'r': mode = 1; break; //read
			case 'l': mode = 2; break; //list
			case 'd': mode = 3; break; //delete file
			case 'e': mode = 4; break; //erase device
			case 'i': mode = 9; break; //info
			//case 'v': verbose = 1; break;
			default:
				fprintf(stderr, "Unknown flag '%c'\n\n", arg[i]);
				usage(NULL);
		}
	}
}

void parse_options(int argc, char **argv)
{
	int i;
	char *arg;

	for (i=1; i<argc; i++) {
		arg = argv[i];

		if(arg[0] == '-') {
			if(arg[1] == '-') {
				char *name = &arg[2];
				char *val  = strchr(name, '=');
				if(val == NULL) {
					//value must be the next string.
					val = argv[++i];
				}
				else {
					//we found an =, so split the string at it.
					*val = '\0';
					 val = &val[1];
				}

			}
			else parse_flag(arg);
		}
		else if (strcmp("serflash", arg) == 0) device = 0;
		else if (strcmp("parflash", arg) == 0) device = 2;
		else if (strcmp("dev7", arg) == 0) device = 135;
		else if (strcmp("dev6", arg) == 0) device = 134;
		else if (strcmp("dev5", arg) == 0) device = 133;
		else if (strcmp("dev4", arg) == 0) device = 132;
		else if (strcmp("dev3", arg) == 0) device = 131;
		else if (strcmp("dev2", arg) == 0) device = 130;
		else if (strcmp("dev1", arg) == 0) device = 129;
		else if (strcmp("dev0", arg) == 0) device = 128;
		else if (strcmp("eeprom", arg) == 0) device = 253;
		else if (strcmp("teensy", arg) == 0) device = 254;
//		else if (strcmp("sdcard", arg) == 0) device = 3; todo...
		else fname = arg;
	}

	//printf("mode:%d device:%d verbose:%d filename:%s\n", mode, device, verbose, fname);
}
/************************************************************************************************************/

void commErr() {
	die("Communication error");
}

int hid_sendAck(void) {
  return rawhid_send(hid_device, buf, sizeof(buf), timeout);
}

int hid_checkAck() {
char buf2[sizeof(buf)];
int n;
	n = rawhid_recv(hid_device, buf2, sizeof(buf2), timeout);
	if (n < 1) return n;
	n = memcmp(buf, buf2, sizeof(buf));
	return n;
}

void hid_sendWithAck() {
char buf2[sizeof(buf)];
int n;
	n = rawhid_send(hid_device, buf, sizeof(buf), timeout);
	if (n < 1) commErr();
	n = rawhid_recv(hid_device, buf2, sizeof(buf2), timeout);
	if (n < 1) commErr();
	n = memcmp(buf, buf2, sizeof(buf));
	if (n) commErr();
	return;
}

void hid_rcvWithAck() {
int n;
	n = rawhid_recv(hid_device, buf, sizeof(buf), timeout);
	if (n < 1) commErr();
	n = rawhid_send(hid_device, buf, sizeof(buf), timeout);
	if (n < 1) commErr();
}

/***********************************************************************************************************
  serial flash / parallel flash
************************************************************************************************************/


void serflash_write(void);
void serflash_read(void);
void serflash_list(void);
void serflash_delfile(void);
void serflash_erase(void);
void serflash_info(void);

int serflash_ready(void) {
	buf[0] = 99;
	buf[1] = device;
	hid_sendWithAck();
	rawhid_recv(hid_device, buf, sizeof(buf), timeout);
	if (buf[1] != 17 /*magic*/) die("Teensy is in a wrong state. Please restart it.");
	return buf[0];
}

void serflash() {
  //first, check access
  if (serflash_ready()!=0) die("busy.");

  switch (mode) {
	case 0 : serflash_write(); break;
	case 1 : serflash_read();break;
	case 2 : serflash_list();break;
	case 3 : serflash_delfile();break;
	case 4 : serflash_erase();break;
	case 9 : serflash_info();break;
	default: die("Command not supported by device");
  }
}

void serflash_write(void) {
  FILE *fp;
  int sz,r, pos, dotcnt;
  char * buffer;
  char *basec, *bname;

	fp = fopen(fname, "rb");
	if (fp == NULL) {
		fprintf(stderr, "Unable to open %s\n\n", fname);
		usage(NULL);
	}

	//get size of file
	fseek(fp, 0, SEEK_END);
	sz = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	//if (verbose) printf("Size of file is %d bytes.\n",sz);

	if (sz == 0) die("");

	memset(buf, 0, sizeof(buf));
	buf[0] = mode;
	buf[1] = device;
	buf[2] = (sz >> 24) & 0xff;
	buf[3] = (sz >> 16) & 0xff;
	buf[4] = (sz >> 8) & 0xff;
	buf[5] = sz & 0xff;
	hid_sendWithAck();

	basec = strdup(fname);
	bname = basename(basec);

	r =  MIN(strlen(bname),63);
	strncpy((char*)buf, bname, r);
	buf[r]=0;
	hid_sendWithAck();

	//Todo: check for free space on flash here

	// allocate memory to contain the whole file:
	buffer = (char*) malloc (sizeof(char) * sz);
	if (buffer == NULL) die ("Memory error");

	// copy the file into the buffer:
	r = fread (buffer, 1, sz, fp);
	if (r != sz) die("File read error");

	fclose (fp);

	//Transfer the file to the Teensy
	printf(".");
	dotcnt = 0;
	pos = 0;
	while (pos < sz) {
		int c = MIN(sizeof(buf), sz-pos);
		memset(buf, 0xff, sizeof(buf));
		memcpy(buf, buffer + pos, c);
		pos += c;
		r = rawhid_send(hid_device, buf, sizeof(buf), timeout*2);
		if (r < 0 ) die("Transfer error");
		if (++dotcnt > DOTCNTPACKETS) {
			printf(".");
			fflush(stdout);
			dotcnt = 0;
		}
	}
	free (buffer);

	if (hid_checkAck() != 0)  commErr() ;
	printf("\n");

};

void serflash_read(void) {
  unsigned sz, pos;
  int r;

	//printf("Read\r\n");
	memset(buf, 0, sizeof(buf));
	buf[0] = mode;
	buf[1] = device;
	hid_sendWithAck();

	strncpy((char*)buf, fname, sizeof(buf));
	hid_sendWithAck();

	//r = rawhid_recv(hid_device, buf, sizeof(buf), 100);
	hid_rcvWithAck();
	//if (r < 1)  commErr() ;
	if (buf[0]==2) die("File not found");
	else if (buf[0]!=1)  commErr() ;
	//hid_sendAck();

	sz = (buf[1] << 24) | (buf[2] << 16) | (buf[3] << 8) | buf[4];
	//printf("size:%d %d %d %d %d\r\n",sz, buf[1], buf[2], buf[3], buf[4]);

	pos = 0;
	do {
		hid_rcvWithAck();
		r = sz - pos;
		if (r > sizeof(buf)) r = sizeof(buf);
		pos+= r;
		if (r) fwrite(buf, r, 1, stdout);
	} while (pos<sz);

};

void serflash_list(void) {
uint32_t sz;
	//printf("List\r\n");
	memset(buf, 0, sizeof(buf));
	buf[0] = mode;
	buf[1] = device;
	hid_sendWithAck();

	while (1) {
		hid_rcvWithAck();
		if (buf[0]==0) break;
		sz = (buf[1] << 24) | (buf[2] << 16) | (buf[3] << 8) | buf[4];
		hid_rcvWithAck();
		printf("%8d %s\n", sz, buf);
	}


};

void serflash_delfile(void) {
uint32_t n;
	//printf("del file \n");
	memset(buf, 0, sizeof(buf));
	buf[0] = mode;
	buf[1] = device;
	hid_sendWithAck();

	strncpy((char*)buf, fname, sizeof(buf));
	rawhid_send(hid_device, buf, sizeof(buf), timeout);


	n= rawhid_recv(hid_device, buf, sizeof(buf), timeout);
	if (n < 1) commErr();
	if (buf[0]==0) die("File not found");

}



void serflash_erase(void) {
const int timeToSleep = 250;
int t = 0;
	//printf("erase chip\n");
	memset(buf, 0, sizeof(buf));
	buf[0] = mode;
	buf[1] = device;
	hid_sendWithAck();

	do {
		DELAY( timeToSleep );
		if (serflash_ready()==0) break;
		if (++t >= 1000 /  timeToSleep / 2) {
			printf(".");
			fflush(stdout);
			t = 0;
		}
	} while (1);
	printf("\n");

};

void serflash_info(void) {
int n;
uint32_t sz;
//	printf("info");
	buf[0] = mode;
	buf[1] = device;
	hid_sendWithAck();

	n = rawhid_recv(hid_device, buf, sizeof(buf), timeout);
	if (n < 1) commErr();

	printf("ID    : %02X %02X %02X\n", buf[8], buf[9], buf[10] );
	printf("Serial: %02X %02X %02X %02X %02X %02X %02X %02X\n", buf[16],buf[17],buf[18],buf[19],buf[20],buf[21],buf[22],buf[23]);
	sz = (buf[1] << 24) | (buf[2] << 16) | (buf[3] << 8) | buf[4];
	printf("Size  : %d Bytes\n", sz);
	//debug();
};

/***********************************************************************************************************
  eeprom
************************************************************************************************************/

void eeprom_write(void);
void eeprom_read(void);

void eeprom(void) {
  switch (mode) {
	case 0 : eeprom_write(); break;
	case 1 : eeprom_read();break;
	//case 4 : extflash_erase();break; todo...
	default: die("Command not supported by device");
  }

}

void eeprom_write(void) {
  FILE *fp;
  int sz, szee, r, pos;
  char * buffer;
	//printf("write ee\n");
	fp = fopen(fname, "rb");
	if (fp == NULL) {
		fprintf(stderr, "Unable to read %s\n\n", fname);
		usage(NULL);
	}

	//get size of file
	fseek(fp, 0, SEEK_END);
	sz = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	//if (verbose) printf("Size of file is %d bytes.\n",sz);

	if (sz == 0) die("");

	memset(buf, 0, sizeof(buf));
	buf[0] = mode;
	buf[1] = device;
	hid_sendWithAck();

	//check size of eeprom
	hid_rcvWithAck();
	szee = (buf[1] << 24) | (buf[2] << 16) | (buf[3] << 8) | buf[4];
	if (szee < sz) die("File is too large");
	if (szee < sizeof(buf)) die("Minimum EEPROM size is 64 Bytes" );

	// allocate memory to contain the whole file:
	buffer = (char*) malloc (sizeof(char) * sz);
	if (buffer == NULL) die ("Memory error");

	// copy the file into the buffer:
	r = fread (buffer, 1, sz, fp);
	if (r != sz) die("File reading error");

	fclose (fp);

	//Transfer the file to the Teensy
	pos = 0;
	while (pos < sz) {
		int c = MIN(sizeof(buf), sz-pos);
		memset(buf, 0xff, sizeof(buf));
		memcpy(buf, buffer + pos, c);
		pos += c;
		r = rawhid_send(hid_device, buf, sizeof(buf), timeout*2);
		if (r < 0 ) die("Transfer error");
	}
	free (buffer);

	if (hid_checkAck() != 0)  commErr() ;

};

void eeprom_read(void) {
  unsigned sz, pos;
  int r;

//	printf("Read\r\n");
	memset(buf, 0, sizeof(buf));
	buf[0] = mode;
	buf[1] = device;
	hid_sendWithAck();

	hid_rcvWithAck();
	sz = (buf[1] << 24) | (buf[2] << 16) | (buf[3] << 8) | buf[4];


	pos = 0;

	do {
		hid_rcvWithAck();
		r = sz - pos;
		if (r > sizeof(buf)) r = sizeof(buf);
		pos+= r;
		if (r) fwrite(buf, r, 1, stdout);
	} while (pos<sz);
}

/***********************************************************************************************************
  Teensy
************************************************************************************************************/
void teensy(void) {
  unsigned sz;
	if (mode!=9) die("Command not supported by device");
	buf[0] = mode;
	buf[1] = device;
	hid_sendWithAck();
	rawhid_recv(hid_device, buf, sizeof(buf), timeout);

	printf("Model : ");
	switch (buf[0]) {
		case 1: printf("Teensy 3.0 (MK20DX128)");break;
		case 2: printf("Teensy 3.1/3.2 (MK20DX256)");break;
		case 3: printf("Teensy LC (MKL26Z64)");break;
		case 4: printf("Teensy 3.5 (MK64FX512)");break;
		case 5: printf("Teensy 3.6 (MK66FX1M0)");break;
		default: printf("unknown");break;
	}
	printf("\n");

	sz = buf[61]<<16 | buf[62]<<8 | buf[63];
	if (sz < 10000000) sz = sz * 10;
	printf("Serial: %d\n",sz);

	printf("MAC   : %02X:%02X:%02X:%02X:%02X:%02X\n",buf[58],buf[59],buf[60],buf[61],buf[62],buf[63]);

	sz = (buf[1] << 24) | (buf[2] << 16) | (buf[3] << 8) | buf[4];
	printf("EEPROM: %d Bytes\n",sz);

	sz = (buf[16] << 24) | (buf[17] << 16) | (buf[18] << 8) | buf[19];
	printf("F_CPU : %d Hz\n",sz);

	sz = (buf[20] << 24) | (buf[21] << 16) | (buf[22] << 8) | buf[23];
	printf("F_PLL : %d Hz\n",sz);

	sz = (buf[24] << 24) | (buf[25] << 16) | (buf[26] << 8) | buf[27];
	printf("F_BUS : %d Hz\n",sz);

	sz = (buf[28] << 24) | (buf[29] << 16) | (buf[30] << 8) | buf[31];
	printf("F_MEM : %d Hz",sz);
	//debug();
}

/************************************************************************************************************/
int main(int argc, char **argv)
{

#if defined(OS_WINDOWS)
	setmode(fileno(stdout), O_BINARY);
#endif

	parse_options(argc, argv);

	if ( (mode == 0 || mode == 1 || mode == 3) && (fname==NULL)  && (device<200)) usage("Filename required");
	if ((mode==2 || mode == 4 || mode==9) && fname != NULL) usage("Filename not allowed");


	int r;
	r = rawhid_open(1, 0x16C0, 0x0486, 0xFFAB, 0x0200);
	if (r <= 0) {
		die("no rawhid device found\n");
	}

	//Get ID-data from Teensy
	buf[0] = 9;
	buf[1] = 254;
	hid_sendWithAck();
	rawhid_recv(hid_device, buf, sizeof(buf), timeout);

	//Check version
	#if 1
	int teensyversion;
	teensyversion = (buf[10] << 24) | (buf[11] << 16) | (buf[12] << 8) | buf[13];
	if (VERSION !=  teensyversion) {
		die("version conflict.\nPC (this): %d\nTeensy   : %d\n", VERSION, teensyversion);
	}
	#endif

	//Check Teensy-compiled-in devices
	unsigned int teensydevices;
	teensydevices =  (buf[6] << 24) | (buf[7] << 16) | (buf[8] << 8) | buf[9];
	if (device == 253 && !(teensydevices & 2)) die("Teensy has no eeprom support compiled-in.\nCheck TeensyTransfer.h");
	if (device == 0 && !(teensydevices & 4)) die("Teensy has no serflash support compiled-in.\nCheck TeensyTransfer.h");
	if (device == 2 && !(teensydevices & 8)) die("Teensy has no parflash support compiled-in.\nCheck TeensyTransfer.h");


	//clock_t t = clock();

	switch(device) {
		case 0:
		case 2:
		case 128:
		case 129:
		case 130:
		case 131:
		case 132:
		case 133:
		case 134:
		case 135: serflash();break;
		case 253: eeprom();break;
		case 254: teensy();break;
		default: usage(NULL);
	}
	/*
	{
		t = clock() - t;
		double time_taken = ((double)t)/CLOCKS_PER_SEC; // in seconds
		printf("it took %f seconds to execute \n", time_taken);
	}
	*/

	exit(0);
}


