#CC=gcc
#CFLAGS=-Wall -Os -g

all:
	$(CC) $(CFLAGS) -c -o dsmr2mqtt.o dsmr2mqtt.c
	$(CC) $(CFLAGS) dsmr2mqtt.o -Ldsmr-p1-parser -ldsmr -lmosquitto -o dsmr2mqtt

clean:
	rm -rf dsmr2mqtt.o dsmr2mqtt
