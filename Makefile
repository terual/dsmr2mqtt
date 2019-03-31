CC ?= gcc
CFLAGS ?= -Wall -Os -g

all: dsmr2mqtt

dsmr2mqtt: dsmr-p1-parser dsmr2mqtt.o
	$(CC) $(CFLAGS) dsmr2mqtt.o -Ldsmr-p1-parser -ldsmr -lmosquitto -o dsmr2mqtt

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

dsmr-p1-parser:
	$(MAKE) -C $@ libdsmr.a

.PHONY: clean dsmr-p1-parser

clean:
	$(MAKE) -C dsmr-p1-parser clean
	rm -rf dsmr2mqtt.o dsmr2mqtt
