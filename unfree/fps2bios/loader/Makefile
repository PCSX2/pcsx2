#  _____     ___ ____
#   ____|   |    ____|      PSX2 OpenSource Project
#  |     ___|   |____       (C)2002, David Ryan ( Oobles@hotmail.com )
#  ------------------------------------------------------------------------

LOADER_BIN = ../build/LOADER

EE_INCS += -Iinclude

EE_LIBS += -lkernel -lmc -lkernel -lpad -lc -lkernel 

EE_OBJS = loader.o menu.o eedebug.o crt0.o romdir.o

$(LOADER_BIN): $(EE_OBJS)
	$(EE_CC) -nostartfiles -Tlinkfile $(EE_CFLAGS) \
		-o $(LOADER_BIN) $(EE_OBJS) $(EE_LDFLAGS) $(EE_LIBS)

clean:
	rm -f -r $(EE_OBJS) $(LOADER_BIN)

include $(PS2SDK)/Defs.make
include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
