# dsmr2mqtt

This utility reads the telegrams from your DSMR-compatible smart meter and sends it 
to a MQTT broker. I made use of [dsmr-p1-parser](https://github.com/lvzon/dsmr-p1-parser) and [libmosquitto](https://mosquitto.org/man/libmosquitto-3.html).

I wrote this so I would be able to plug my smart meter into my OpenWRT router with a
prefabricated cable (I used this one: https://www.sossolutions.nl/slimme-meter-kabel).

I used the MQTT topic definitions of [dsmr-reader](https://github.com/dennissiemensma/dsmr-reader).
If you want to use other definitions, you can change the the #defines at the beginning of the *dsmr2mqtt.c* file.

## Compiling

### Debian/Ubuntu

First make sure you have git installed and the necessary library.
```
# apt install git libmosquitto-dev
```

Then get the source code and use the command make to build the dsmr2mqtt executable:
```
$ cd [whatever build dir]
$ git clone https://github.com/terual/dsmr2mqtt
$ cd dsmr2mqtt
$ make
$ ./dsmr2mqtt
```

### Cross-compiling for OpenWRT

To compile this for OpenWRT, use the following [guide](https://openwrt.org/docs/guide-developer/using_the_sdk),
place the Makefile.openwrt in a seperate dsmr2mqtt package folder, and rename it to Makefile.

## Using the tool

```
Usage:  dsmr2mqtt [-d <serial_device>] [-m <mqtt_broker_host>] [-p <mqtt_broker_port>]
    -d <serial_device>      Serial device for DSMR device (default is /dev/ttyUSB0)
    -m <mqtt_broker_host>   Host name for MQTT broker (default is localhost)
    -p <mqtt_broker_port>   Host name for MQTT broker port (default is 1883)
```
