#Modify this to point to the PJSIP location.
PJBASE=/home/vlbrazhnik/wortex/eltex_answer_machine/pjproject-2.10

include $(PJBASE)/build.mak

CC      = $(PJ_CC)
LDFLAGS = $(PJ_LDFLAGS)
LDLIBS  = $(PJ_LDLIBS)
CFLAGS  = $(PJ_CFLAGS)
CPPFLAGS= ${CFLAGS}

# If your application is in a file named myapp.cpp or myapp.c
# this is the line you will need to build the binary.
all: answ_machine  

myapp: answ_machine.c
	$(CC) -o -g -Wall -Wextra $@ $< $(CPPFLAGS) $(LDFLAGS) $(LDLIBS)

clean:
	rm -f answ_machine.o answ_machine
