#  _____     ___ ____
#   ____|   |    ____|      PSX2 OpenSource Project
#  |     ___|   |____       (C)2002, David Ryan ( Oobles@hotmail.com )
#  ------------------------------------------------------------------------

DIRS    = iopboot sysmem loadcore excepman intrman stdio timrman \
		  dmacman sifman sifcmd ssbusc sysclib heaplib \
		  vblank threadman sio2man

iopload: $(OBJECTS)
	for i in $(DIRS); do \
		make -C $$i; \
	done; 

clean:
	for i in $(DIRS); do \
		make -C $$i clean; \
	done; 
