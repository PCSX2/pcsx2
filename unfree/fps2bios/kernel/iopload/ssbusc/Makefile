#  _____     ___ ____
#   ____|   |    ____|      PSX2 OpenSource Project
#  |     ___|   |____       (C)2002, David Ryan ( Oobles@hotmail.com )
#  ------------------------------------------------------------------------

IOP_BIN = ../../../build/SSBUSC

IOP_INCS += -I../include

IOP_OBJS = ssbusc.o ../iopdebug.o ../libkernel/iop_loadcore.o ../libkernel/iop_intrman.o

all: $(IOP_BIN)

clean:
	rm -f -r $(IOP_OBJS) $(IOP_BIN)

include $(PS2SDK)/Defs.make
include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.iopglobal
