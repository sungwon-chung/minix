
ALL = minix ./user_prog/idle ./user_prog/init ./tests/testTerminals ./tests/samples-ossim/bigstack ./tests/samples-ossim/blowstack ./tests/samples-ossim/brktest ./tests/samples-ossim/console ./tests/samples-ossim/delaytest ./tests/samples-ossim/exectest ./tests/samples-ossim/forktest0 ./tests/samples-ossim/forktest1 ./tests/samples-ossim/forktest1b ./tests/samples-ossim/forktest2b ./tests/samples-ossim/forktest2 ./tests/samples-ossim/forktest3 ./tests/samples-ossim/forkwait0c ./tests/samples-ossim/forkwait0p ./tests/samples-ossim/forkwait1 ./tests/samples-ossim/forkwait1b ./tests/samples-ossim/forkwait1c ./tests/samples-ossim/forkwait1d ./tests/samples-ossim/init ./tests/samples-ossim/init2 ./tests/samples-ossim/init3 ./tests/samples-ossim/shell ./tests/samples-ossim/trapillegal ./tests/samples-ossim/trapmath ./tests/samples-ossim/trapmemory ./tests/samples-ossim/ttyread1 ./tests/samples-ossim/ttywrite1 ./tests/samples-ossim/ttywrite2 ./tests/samples-ossim/ttywrite3

KERNEL_OBJS =  ./src/kernel.o ./src/calls.o ./src/contextSwitch.o ./src/loadProgram.o ./src/queue.o ./src/pcbManager.o ./src/traps.o 
KERNEL_SRCS =  ./src/kernel.c ./src/calls.c ./src/contextSwitch.c ./src/loadProgram.c ./src/queue.c ./src/pcbManager.c ./src/traps.c


HEADER_DIRS = . $(PUBLIC_DIR)/include

CPPFLAGS = -I$(PUBLIC_DIR)/include
# CFLAGS = -g -Wall -Wextra -Werror -I/kernel.h  -I/calls.h -I/loadProgram.h -I/contextSwitch.h -I/traps.h
CFLAGS = -g -Wall -Wextra -Werror -Wno-error=div-by-zero -Wno-div-by-zero $(addprefix -I, $(HEADER_DIRS)) -w

LANG = gcc

%: %.o
	$(LINK.o) -o $@ $^ $(LOADLIBES) $(LDLIBS)

LINK.o = $(PUBLIC_DIR)/bin/link-user-$(LANG) $(LDFLAGS) $(TARGET_ARCH)

%: %.c
%: %.cc
%: %.cpp

all: $(ALL)

minix: $(KERNEL_OBJS)
	$(PUBLIC_DIR)/bin/link-kernel-$(LANG) -o minix $(KERNEL_OBJS)

clean:
	rm -f $(KERNEL_OBJS) $(ALL)

depend:
	$(CC) $(CPPFLAGS) -M $(KERNEL_SRCS) > .depend

#include .depend