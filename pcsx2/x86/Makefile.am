INCLUDES = -I@srcdir@/../   -I@srcdir@/../../common/
noinst_LIBRARIES = libx86recomp.a

# have to add the sources instead of making a library since the linking is complicated

archfiles = ix86-32/iR5900-32.cpp   ix86-32/iR5900AritImm.cpp  ix86-32/iR5900Jump.cpp \
ix86-32/iR5900Move.cpp ix86-32/iR5900Shift.cpp ix86-32/iR5900Arit.cpp  ix86-32/iR5900Branch.cpp \
ix86-32/iR5900LoadStore.cpp  ix86-32/iR5900MultDiv.cpp ix86-32/iCore-32.cpp ix86-32/aR5900-32.S 
#ix86-32/iR5900Templates.cpp

libx86recomp_a_SOURCES = iCOP2.cpp  iCP0.cpp  iFPU.cpp  iHw.cpp  iMMI.cpp  iPsxHw.cpp  iPsxMem.cpp  \
ir5900tables.cpp  iVU0micro.cpp  iVU1micro.cpp  iVUmicro.cpp iVUmicroUpper.cpp iVUmicroLower.cpp \
iCore.cpp  iGS.cpp  iR3000A.cpp  iR3000Atables.cpp  iVif.cpp  iVUzerorec.cpp \
fast_routines.S aR3000A.S aVUzerorec.S aVif.S $(archfiles)

libx86recomp_a_DEPENDENCIES = ix86/libix86.a

SUBDIRS = ix86