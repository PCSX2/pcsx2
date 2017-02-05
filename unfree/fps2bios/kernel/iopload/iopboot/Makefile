#  _____     ___ ____
#   ____|   |    ____|      PSX2 OpenSource Project
#  |     ___|   |____       (C)2002, David Ryan ( Oobles@hotmail.com )
#  ------------------------------------------------------------------------

IOPBOOT_BIN = ../../../build/IOPBOOT

IOP_INCS += -I../include

IOP_OBJS = iopboot.o ../iopdebug.o ../iopelf.o ../romdir.o 

$(IOPBOOT_BIN): $(IOP_OBJS)
	$(EE_CC) -Wl,--oformat,binary,--Map,iopboot.map -T linkfile $(IOP_OBJS) -nostartfiles -o $(IOPBOOT_BIN)

clean:
	rm -f -r $(IOP_OBJS) $(IOPBOOT_BIN)

include $(PS2SDK)/Defs.make
include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.iopglobal
