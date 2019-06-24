CC=gcc
CPPFLAGS=-g
DAEMON=runns
CLIENT=runnsctl

all: $(DAEMON) $(CLIENT)

$(DAEMON): runns.o
	$(CC) -o $@ $<

$(CLIENT): client.o
	$(CC) -o $@ $<

.PHONY: client
clean:
	rm -f $(DAEMON) $(CLIENT) *.o
