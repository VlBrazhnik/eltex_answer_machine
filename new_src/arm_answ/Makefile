#Modify this to point to the PJSIP location.
PJBASE=/home/vlbrazhnikov/Local_Rep/eltex_answer_machine/pjproject-2.11.1

include $(PJBASE)/build.mak

CC      = arm-marvell-linux-gnueabi-gcc 
LDFLAGS = $(PJ_LDFLAGS)
LDLIBS  = $(PJ_LDLIBS)
CPPFLAGS= ${CFLAGS}
CFLAGS  = $(PJ_CFLAGS)

# If your application is in a file named myapp.cpp or myapp.c
# this is the line you will need to build the binary.
all: arm_answ_machine  

myapp: arm_answ_machine.c
	$(CC) -o $@ $< $(LDFLAGS) $(LDLIBS) $(CPPFLAGS)

clean:
	rm -f arm_answ_machine.o arm_answ_machine
