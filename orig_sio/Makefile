# SIO_NTAP MAKEFILE
all: 
	echo "make [solaris|solaris64|hpux|aix|linux|freebsd|windows]"

# HPUX

HPUX_CC = /opt/ansic/bin/cc
HPUX_CFLAGS = -Ae
HPUX_LIBS = -lpthread

hpux_ia64:
		make hpux
		mv sio_ntap_hpux sio_ntap_hpux_ia64
                                                                               
hpux_parisc:
		make hpux
		mv sio_ntap_hpux sio_ntap_hpux_parisc
                                                                                

hpux:		sio_ntap.c
		${HPUX_CC} -o sio_hpux.o -c sio_ntap.c ${HPUX_CFLAGS} -D_HPUX
		${HPUX_CC} ${HPUX_CFLAGS} -o sio_ntap_hpux sio_hpux.o ${HPUX_LIBS}
		rm sio_hpux.o

# AIX last compiled on aix-rtp (10.60.192.220) an rtp CS machine

AIX_CC = /usr/bin/gcc
AIX_CFLAGS = 
AIX_LIBS = -lpthread

aix:		sio_ntap.c
		${AIX_CC} -o sio_aix.o -c sio_ntap.c ${AIX_CFLAGS} -D_AIX
		${AIX_CC} ${AIX_CFLAGS} -o sio_ntap_aix sio_aix.o ${AIX_LIBS}
		rm sio_aix.o

# LINUX last compiled on aix-rtp (10.60.192.220) an rtp CS machine

LINUX_CC = gcc
LINUX_CFLAGS = -g -D_GNU_SOURCE
LINUX_LIBS = -lpthread

linux:		sio_ntap.c
		${LINUX_CC} -o sio_linux.o -c sio_ntap.c ${LINUX_CFLAGS} -D__LINUX__
		${LINUX_CC} ${LINUX_CFLAGS} -o sio_ntap_linux sio_linux.o ${LINUX_LIBS}
		rm sio_linux.o

# FREEBSD last compiled on freebird  

FREEBSD_CC = gcc
FREEBSD_CFLAGS = -g -pthread
FREEBSD_LIBS = -lcompat

freebsd:	sio_ntap.c
		${FREEBSD_CC} -o sio_freebsd.o -c sio_ntap.c ${FREEBSD_CFLAGS} 
		${FREEBSD_CC} ${FREEBSD_CFLAGS} -o sio_ntap_freebsd sio_freebsd.o ${FREEBSD_LIBS}
		rm sio_freebsd.o

# Solaris (32 and 64bit builds)

SOL_CC = /opt/SUNWspro/bin/cc
SOL_CFLAGS = -xCC -D_REENTRANT -D_SOLARIS
SOL64_CFLAGS = -xCC -D_REENTRANT -D_SOLARIS -xarch=v9
SOL_LIBS = -lpthread -lposix4 -laio -lkstat -lrt -lpthread -R /usr/lib/lwp

solaris:	sio_ntap.c
		${SOL_CC} ${SOL_CFLAGS} -o sio_solaris.o -c sio_ntap.c -D_SOLARIS
		${SOL_CC} ${SOL_CFLAGS} -o sio_ntap_sol sio_solaris.o ${SOL_LIBS}
		rm sio_solaris.o

solaris64:	sio_ntap.c
		${SOL_CC} ${SOL64_CFLAGS} -o sio_solaris64.o -c sio_ntap.c -D_SOLARIS
		${SOL_CC} ${SOL64_CFLAGS} -o sio_ntap_sol64 sio_solaris64.o ${SOL_LIBS}
		rm sio_solaris64.o

# Windows (must use the Make_win32.bat)
windows: 
	echo "Must run Make_win32.bat from a Windows platform"
