#  _____     ___ ____
#   ____|   |    ____|      PSX2 OpenSource Project
#  |     ___|   |____       (C)2002, David Ryan ( Oobles@hotmail.com )
#  ------------------------------------------------------------------------

EELOAD_BIN = ../../build/EELOAD

EE_INCS += -Iinclude

EE_LIBS += -lmc -lpad -lc -lkernel

EE_OBJS = eeirq.o eedata.o eekernel.o eeinit.o eeload.o eeelf.o eedebug.o romdir.o

$(EELOAD_BIN): $(EE_OBJS) $(PS2SDK)/ee/startup/crt0.o
	$(EE_CC) -nostartfiles -Wl,--oformat,binary,--Map,eeload.map -Tlinkfile $(EE_CFLAGS) \
		-o $(EELOAD_BIN) $(EE_OBJS) $(EE_LDFLAGS) $(EE_LIBS)

# restrict all but the kernel registers
eeirq.o: eeirq.c
	$(EE_C_COMPILE) -ffixed-2 -ffixed-3 -ffixed-4 -ffixed-5 -ffixed-6 -ffixed-7 -ffixed-8 -ffixed-9 -ffixed-10 -ffixed-11 -ffixed-12 -ffixed-13 -ffixed-14 -ffixed-15 -ffixed-16 -ffixed-17 -ffixed-18 -ffixed-19 -ffixed-20 -ffixed-21 -ffixed-22 -ffixed-23 -ffixed-24 -ffixed-25 -fcall-used-26 -fcall-used-27 -ffixed-30 $< -c -o $@

clean:
	rm -f -r $(EELOAD_BIN)

include $(PS2SDK)/Defs.make
include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
