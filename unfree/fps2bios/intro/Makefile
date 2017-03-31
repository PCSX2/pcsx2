#  _____     ___ ____
#   ____|   |    ____|      PSX2 OpenSource Project
#  |     ___|   |____       (C)2002, David Ryan ( Oobles@hotmail.com )
#  ------------------------------------------------------------------------

INTRO_BIN = ../build/INTRO

EE_INCS += -Iinclude

EE_LIBS += -lmc -lpad -lc -lkernel -lm

EE_OBJS = intro.o eedebug.o crt0.o romdir.o

$(INTRO_BIN): $(EE_OBJS)
	$(EE_CC) -nostartfiles -Tlinkfile $(EE_CFLAGS) \
		-o $(INTRO_BIN) $(EE_OBJS) $(EE_LDFLAGS) $(EE_LIBS)

clean:
	rm -f -r $(EE_OBJS) $(INTRO_BIN)

include $(PS2SDK)/Defs.make
include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
