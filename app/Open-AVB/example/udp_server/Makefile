CC?=gcc
OPT=-O2 -g
CFLAGS=$(OPT) -Wall -Wextra -Wno-parentheses
LDLIBS= -lrt -lm -pthread
LDFLAGS=-L../../lib/igb

all: udp_server

udp_server: udp_server.o

udp_server.o: udp_server.c
	$(CC) $(CFLAGS) -c udp_server.c

clean:
	$(RM) udp_server
	$(RM) `find . -name "*~" -o -name "*.[oa]" -o -name "\#*\#" -o -name TAGS -o -name core -o -name "*.orig"`
