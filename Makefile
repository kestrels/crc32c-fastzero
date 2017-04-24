# Copyright (C) 2017 International Business Machines Corp.
# All rights reserved.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version
# 2 of the License, or (at your option) any later version.

AR=ar
RANLIB=ranlib
CC=gcc
CCFLAGS=-c -Wall -Werror -fpic -maltivec -DHAVE_POWER8
CCFLAGS_MIN=-maltivec -DHAVE_POWER8

INCDIR=include

LIBSRC=crc32c_ppc.c
LIBASMSRC=crc32c_ppc_fast_zero_asm.S crc32c_ppc_asm.S
LIBOBJ=$(LIBSRC:.c=.o)
LIBASMOBJ=$(LIBASMSRC:.S=.o)
LIBNAME=libcrc32c.so
LIBNAME_STATIC=libcrc32c.a

LIBDIR=./
LIBSHORTNAME=-lcrc32c
LIBRT=-lrt

PROGSRC=main.c
PROGOBJ=$(PROGSRC:.c=.o)
PROGNAME=main

.PHONY: all clean static

# If you just run 'make' with no options, that implies 'make all'.  
# We will call this the default build, and it will build everything 
# for a shared lib and main.  
all: default-build

# If you run 'make static' that will run the following build steps, 
# which would otherwise be skipped in the default build.  These 
# will result in a static lib and main.
static: static-build

# Create position independent code for each .c and .S file.
do-pic: $(LIBSRC) $(LIBASMSRC)
	$(CC) $(CCFLAGS) -I $(INCDIR) $(LIBSRC) $(LIBASMSRC)

# Create a static lib using the archive and ranlib steps.
static-lib-archive: do-pic
	$(AR) r $(LIBNAME_STATIC) $(LIBOBJ) $(LIBASMOBJ)
	$(RANLIB) $(LIBNAME_STATIC)

# Compile a shared lib.
shared-lib-archive: do-pic
	$(CC) -shared -o $(LIBNAME) $(LIBOBJ) $(LIBASMOBJ)

# Overall we need shared-lib-archive step as a pre-req, then call 'make main'.
default-build: shared-lib-archive
	@$(MAKE) --no-print-directory $(PROGNAME)

static-build: static-lib-archive
	@$(MAKE) --no-print-directory main-static

$(PROGOBJ): $(PROGSRC)
	$(CC) -c $(CCFLAGS_MIN) $(PROGSRC) -o $(PROGOBJ)

# Create 'main' for use with shared library.
# Reminder: In order to find the shared lib at runtime, you need to 
# run 'export LD_LIBRARY_PATH=$PWD' before executing main.
# The $^ is the (space separated) name of all pre-reqs.  
$(PROGNAME): $(PROGOBJ)
	$(CC) $(CCFLAGS_MIN) $^ -L $(LIBDIR) $(LIBSHORTNAME) $(LIBRT) -o $(PROGNAME)

# Create 'main' linked to static library.
# The $^ is the (space separated) name of all pre-reqs.  
main-static:$(PROGOBJ)
	$(CC) -static $(CCFLAGS_MIN) $^ -L $(LIBDIR) $(LIBSHORTNAME) $(LIBRT) -o $(PROGNAME)

clean:
	rm -f $(PROGNAME) $(LIBNAME) $(LIBNAME_STATIC)
	rm -f $(LIBOBJ) $(LIBASMOBJ) $(PROGOBJ)
