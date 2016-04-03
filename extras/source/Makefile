
#OS = LINUX
OS = MACOSX
#OS = WINDOWS

PROG = teensytransfer

# To set up Ubuntu Linux to cross compile for Windows:
#
#    apt-get install mingw32 mingw32-binutils mingw32-runtime
#
# Just edit the variable above for WINDOWS, then use "make" to build rawhid.exe

ifeq ($(OS), LINUX)
TARGET = $(PROG)
CC = gcc
STRIP = strip
CFLAGS = -Wall -O2 -DOS_$(OS)
LIBS = -lusb
PACK = gzip -k $(PROG)
else ifeq ($(OS), MACOSX)
TARGET = $(PROG).mac.zip 
SDK = /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.11.sdk/
ARCH = -mmacosx-version-min=10.5 -arch i386
CC = gcc
STRIP = strip
CFLAGS = -Wall -O2 -DOS_$(OS) -isysroot $(SDK) $(ARCH)
LIBS = $(ARCH) -Wl,-syslibroot,$(SDK) -framework IOKit -framework CoreFoundation
else ifeq ($(OS), WINDOWS)
TARGET = $(PROG).exe
CC = i586-mingw32msvc-gcc
STRIP = i586-mingw32msvc-strip
CFLAGS = -Wall -O2 -DOS_$(OS) 
LIBS = -lhid -lsetupapi 
PACK = zip -9 $(PROG).zip $(PROG).exe 
endif

OBJS = $(PROG).o hid.o


all: $(TARGET)

$(PROG): $(OBJS)
	$(CC) -o $(PROG) $(OBJS) $(LIBS)
#	cp $(PROG) /tmp/$(PROG)
#	$(STRIP) /tmp/$(PROG) 
#	$(PACK)
	

$(PROG).exe: $(PROG)
	mv $(PROG) $(PROG).exe
	$(PACK)

$(PROG).dmg: $(PROG)
	mkdir -p tmp
	cp $(PROG) tmp
	hdiutil create -ov -volname "Raw HID Test" -srcfolder tmp $(PROG).dmg

$(PROG).mac.zip: $(PROG)
	zip $(PROG).mac.zip $(PROG)

hid.o: hid_$(OS).c hid.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o $(PROG) $(PROG).exe $(PROG).dmg $(PROG).mac.zip $(PROG).zip $(PROG).gz
	rm -rf tmp

