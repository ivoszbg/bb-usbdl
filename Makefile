CC      ?= gcc
CFLAGS  ?= -g -O0 -Wall
LDFLAGS ?=
LDLIBS  ?= -lusb-1.0 -lssl -lcrypto

OBJS  = bb-usbdl.o
BIN   = bb-usbdl

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

.PHONY: clean
clean:
	rm $(BIN) $(OBJS)
