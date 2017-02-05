
VERSION = 0
BUILD = 1

CC = gcc

CFLAGS = -Wall -O2 -I.
DIRS = kernel intro loader
FILES = RESET ROMDIR ROMVER IOPBOOT EELOAD \
		SYSMEM LOADCORE EXCEPMAN INTRMAN SSBUSC DMACMAN \
		TIMRMAN SYSCLIB HEAPLIB THREADMAN VBLANK STDIO \
		SIFMAN SIFCMD SIO2MAN LOADER INTRO IOPBTCONF FP2BLOGO 

.PHONY: clean

build/fps2bios: build/ps2romgen_exe build/romdir_exe build/romver_exe | build 
	for i in $(DIRS); do \
		make -C $$i; \
	done;
	cp -f FP2BLOGO build
	cp -f IOPBTCONF build
	(cd build; \
	./romver_exe $(VERSION) $(BUILD); \
	./romdir_exe $(FILES); \
	./ps2romgen_exe fps2bios; \
	cd ..)

build/ps2romgen_exe: ps2romgen.c | build 
	$(CC) $(CFLAGS) $< -o $@

build/romdir_exe: romdir.c | build 
	$(CC) $(CFLAGS) $< -o $@

build/romver_exe: romver.c | build 
	$(CC) $(CFLAGS) $< -o $@

build:
	mkdir -p $@

clean:
	rm -f -r *.o build
	for i in $(DIRS); do \
		make -C $$i clean; \
	done; 
