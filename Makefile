C = gcc
DEFINES = 
DEBUG = -g
#-tui -DNOISY DEBUG
CFLAGS = $(DEFINES) $(DEBUG) -Wall -Wextra -Wshadow -Wunreachable-code -Wredundant-decls -Wmissing-declarations \
	-Wold-style-definition -Wmissing-prototypes -Wdeclaration-after-statement -Wno-return-local-addr \
	-Wunsafe-loop-optimizations -Wuninitialized -Werror

PROG1 = blaissh
PROGS = $(PROG1)

all: $(PROGS)

$(PROG1): $(PROG1).o
	$(CC) $(CFLAGS) -o $(PROG1) $(PROG1).o
	chmod og-rx $(PROG1)

$(PROG1).o: $(PROG1).c
	$(CC) $(CFLAGS) -c $(PROG1).c

clean cls:
	rm -f $(PROG1) *.o *~ \#*

TAR_FILE = ${LOGNAME}_$(PROG1).tar.gz
tar:
	rm -f $(TAR_FILE)
	tar cvaf $(TAR_FILE) blaissh.c *.h [Mm]akefile
	tar tvaf $(TAR_FILE)


