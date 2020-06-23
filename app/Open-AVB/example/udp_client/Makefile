CC?=gcc
OPT=-O2 -g
CFLAGS=$(OPT) -Wall -Wextra -Wno-parentheses
LDLIBS= -lrt -lm -pthread

all: udp_client

udp_client: udp_client.o

udp_client.o: udp_client.c
	$(CC) $(CFLAGS) -c udp_client.c

clean:
	$(RM) udp_client
	$(RM) `find . -name "*~" -o -name "*.[oa]" -o -name "\#*\#" -o -name TAGS -o -name core -o -name "*.orig"`
