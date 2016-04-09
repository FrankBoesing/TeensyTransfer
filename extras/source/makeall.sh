#!/bin/bash
set -e

# UBUNTU script to compile for LINUX, WINDOWS and MAC
#
# To set up Ubuntu Linux to cross compile for Windows:
#
#    apt-get install mingw32 mingw32-binutils mingw32-runtime
#
#
# MacOSX Crosscompiler:
#
# https://launchpad.net/~flosoft/+archive/ubuntu/cross-apple
# add to sources, then:
#	apt-get install apple-ppc-gcc apple-x86-gcc


PROG=teensytransfer
CFLAGS=-"Wall -Os"

rm -f *.o $PROG /tmp/$PROG* $PROG*.gz $PROG.zip $PROG.mac.zip

#MAC
OS="MACOSX"
echo "$OS:"
#SDK="/Developer/SDKs/MacOSX10.5.sdk"
SDK="/usr/lib/apple/SDKs/MacOSX10.5.sdk"
ARCH="-mmacosx-version-min=10.5 -arch ppc -arch i386"
CC="/usr/i686-apple-darwin10/bin/gcc"
STRIP="/usr/i686-apple-darwin10/bin/strip"
LIBS="$ARCH -Wl,-syslibroot,$SDK -framework IOKit -framework CoreFoundation"
PACK="zip $PROG.mac.zip $PROG"

$CC $CFLAGS  -DOS_$OS  -isysroot $SDK $ARCH hid.h hid_$OS.c $PROG.c $LIBS -o /tmp/$PROG
$STRIP /tmp/$PROG -o $PROG
$PACK


#LINUX
rm -f *.o $PROG /tmp/$PROG 
OS="LINUX"
echo "$OS:"
CC="gcc"
STRIP="strip"
LIBS="-lusb"
PACK="gzip -v -k $PROG"


$CC $CFLAGS  -DOS_$OS hid.h hid_$OS.c $PROG.c $LIBS -o /tmp/$PROG
$STRIP /tmp/$PROG -o $PROG
$PACK


#WINDOWS
rm -f *.o
OS="WINDOWS"
echo "$OS:"

CC="i586-mingw32msvc-gcc"
STRIP="i586-mingw32msvc-strip"
LIBS="-lhid -lsetupapi"
PACK="zip $PROG.zip $PROG.exe"

$CC $CFLAGS  -DOS_$OS hid.h hid_$OS.c $PROG.c $LIBS -o /tmp/$PROG.exe
$STRIP /tmp/$PROG.exe -o $PROG.exe
$PACK

#clean tmp, keep linux and windows executables
rm -f *.o /tmp/$PROG*

echo "done."