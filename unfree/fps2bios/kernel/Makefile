#  _____     ___ ____
#   ____|   |    ____|      PSX2 OpenSource Project
#  |     ___|   |____       (C)2002, David Ryan ( Oobles@hotmail.com )
#  ------------------------------------------------------------------------

DIRS = eeload iopload

RESET_BIN = ../build/RESET

EE_INCS += -Iinclude

EE_OBJS = eestart.ee.o
IOP_OBJS = iopstart.iop.o romdir.iop.o start.iop.o

all: $(RESET_BIN)
	for i in $(DIRS); do \
		make -C $$i; \
	done; 

$(RESET_BIN): $(EE_OBJS) $(IOP_OBJS)
	$(EE_CC) -Wl,--oformat,binary -Tlinkfile -nostartfiles $(EE_CFLAGS) \
		-o $(RESET_BIN) $(EE_OBJS) $(IOP_OBJS) $(EE_LDFLAGS) $(EE_LIBS)

%.ee.o: %.c
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

%.iop.o: %.c
	$(IOP_CC) $(IOP_CFLAGS) -c $< -o $@

clean:
	for i in $(DIRS); do \
		make -C $$i clean; \
	done; 
	rm -f -r $(RESET_BIN)

include $(PS2SDK)/Defs.make
include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
include $(PS2SDK)/samples/Makefile.iopglobal
