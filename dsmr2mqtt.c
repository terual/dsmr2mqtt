#include <signal.h>
#include <mosquitto.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dsmr-p1-parser/logmsg.h"
#include "dsmr-p1-parser/p1-lib.h"

#define VERSION "0.1"
#define GIT_VERSION "n/a"

#define DSMR_EQUIPMENT_ID  "dsmr/reading/id"
#define DSMR_TIMESTAMP     "dsmr/reading/timestamp"
#define DSMR_E_IN_TARIFF1  "dsmr/reading/electricity_delivered_1"
#define DSMR_E_OUT_TARIFF1 "dsmr/reading/electricity_returned_1"
#define DSMR_E_IN_TARIFF2  "dsmr/reading/electricity_delivered_2"
#define DSMR_E_OUT_TARIFF2 "dsmr/reading/electricity_returned_2"
#define DSMR_P_IN_TOTAL    "dsmr/reading/electricity_currently_delivered"
#define DSMR_P_OUT_TOTAL   "dsmr/reading/electricity_currently_returned"
#define DSMR_P_IN_L1       "dsmr/reading/phase_currently_delivered_l1"
#define DSMR_P_IN_L2       "dsmr/reading/phase_currently_delivered_l2"
#define DSMR_P_IN_L3       "dsmr/reading/phase_currently_delivered_l3"
#define DSMR_P_OUT_L1      "dsmr/reading/phase_currently_returned_l1"
#define DSMR_P_OUT_L2      "dsmr/reading/phase_currently_returned_l2"
#define DSMR_P_OUT_L3      "dsmr/reading/phase_currently_returned_l3"

#define DSMR_DEV_COUNTER_TIMESTAMP "dsmr/reading/extra_device_timestamp"
#define DSMR_DEV_COUNTER   "dsmr/reading/extra_device_delivered"
#define DSMR_DEV_COUNTER_RATE "dsmr/reading/extra_device_delivered_rate"

#define DSMR_P1_VERSION    "dsmr/meter-stats/dsmr_version"
#define DSMR_TARIFF        "dsmr/meter-stats/electricity_tariff"
#define DSMR_POWER_FAILURES "dsmr/meter-stats/power_failure_count"
#define DSMR_POWER_FAILURES_LONG "dsmr/meter-stats/long_power_failure_count"
#define DSMR_V_SAGS_L1     "dsmr/meter-stats/voltage_sag_count_l1"
#define DSMR_V_SAGS_L2     "dsmr/meter-stats/voltage_sag_count_l2"
#define DSMR_V_SAGS_L3     "dsmr/meter-stats/voltage_sag_count_l3"
#define DSMR_V_SWELLS_L1   "dsmr/meter-stats/voltage_swell_count_l1"
#define DSMR_V_SWELLS_L2   "dsmr/meter-stats/voltage_swell_count_l2"
#define DSMR_V_SWELLS_L3   "dsmr/meter-stats/voltage_swell_count_l3"


/* config struct */
struct dsmr2mqtt_config {
  char *serial_device;
  char *mqtt_broker_host;
  int mqtt_broker_port;
};
struct dsmr2mqtt_config config;

/* Mosquitto */
struct mosquitto *mosq = NULL;

/* Global counter for last gas meter value */
double   last_gas_count = 0;
uint32_t last_gas_timestamp = 0;

bool volatile keepRunning = true;

void intHandler(int dummy) {
    keepRunning = 0;
}

/* Help and arguments */
void show_help() {
  fprintf(stderr, "dsmr2mqtt, version %s-%s\n", VERSION, GIT_VERSION);
  fprintf(stderr, "Usage:  dsmr2mqtt [-d <serial_device>] [-m "
                  "<mqtt_broker_host>] [-p <mqtt_broker_port>]\n");
  fprintf(stderr, "    -d <serial_device>      Serial device for DSMR device "
                  "(default is /dev/ttyUSB0)\n");
  fprintf(stderr, "    -m <mqtt_broker_host>   Host name for MQTT broker "
                  "(default is localhost)\n");
  fprintf(stderr, "    -p <mqtt_broker_port>   Host name for MQTT broker port "
                  "(default is 1883)\n");
}

int parse_arguments(int argc, char **argv) {
  int c;
  int digit_optind = 0;
  int aopt = 0, bopt = 0;
  char *copt = 0, *dopt = 0;
  while ((c = getopt(argc, argv, "hd:m:p:")) != -1) {
    int this_option_optind = optind ? optind : 1;
    switch (c) {
    case 'h':
      show_help();
      exit(0);
      break;
    case 'd':
      config.serial_device = optarg;
      break;
    case 'm':
      config.mqtt_broker_host = optarg;
      break;
    case 'p':
      config.mqtt_broker_port = strtoul(optarg, NULL, 10);
      break;
    default:
      show_help;
    }
  }
  return 0;
}

void mosq_log_callback(struct mosquitto *mosq, void *userdata, int level,
                       const char *str) {
  /* Pring all log messages regardless of level. */

  switch (level) {
  // case MOSQ_LOG_DEBUG:
  // case MOSQ_LOG_INFO:
  // case MOSQ_LOG_NOTICE:
  case MOSQ_LOG_WARNING:
  case MOSQ_LOG_ERR: {
    fprintf(stderr, "%i:%s\n", level, str);
  }
  }
}

void mosq_publish_callback(struct mosquitto *mosq, void *userdata, int level) {}

/* Setup MQTT connection to broker */
void mqtt_setup(char *host, int port) {

  int keepalive = 60;
  bool clean_session = true;

  mosquitto_lib_init();
  mosq = mosquitto_new(NULL, clean_session, NULL);
  if (!mosq) {
    fprintf(stderr, "Error: Out of memory.\n");
    exit(1);
  }

  mosquitto_log_callback_set(mosq, mosq_log_callback);
  // mosquitto_publish_callback_set(mosq, mosq_publish_callback);

  if (mosquitto_connect(mosq, host, port, keepalive)) {
    fprintf(stderr, "Unable to connect to MQTT broker on %s:%i.\n", host, port);
    exit(1);
  }

  int loop = mosquitto_loop_start(mosq);
  if (loop != MOSQ_ERR_SUCCESS) {
    fprintf(stderr, "Unable to start loop: %i\n", loop);
    exit(1);
  }

  fprintf(stderr, "Connected to MQTT broker on %s:%i...\n", host, port);
}

int mqtt_send(char *topic, char *msg, bool retain) {
  return mosquitto_publish(mosq, NULL, topic, strlen(msg), msg, 0, retain);
}

int send_values(struct dsmr_data_struct *data) {

  char *msg = malloc(64);

  /* Current timestamp */
  sprintf(msg, "%i", data->timestamp);
  mqtt_send(DSMR_TIMESTAMP, msg, 0);

  /* Current electricity delivered in Watts */
  sprintf(msg, "%.0f", data->P_in_total * 1000);
  mqtt_send(DSMR_P_IN_TOTAL, msg, 0);

  /* Current electricity returned in Watts */
  sprintf(msg, "%.0f", data->P_out_total * 1000);
  mqtt_send(DSMR_P_OUT_TOTAL, msg, 0);

  /*  in Watts */
  sprintf(msg, "%.0f", data->P_in[0] * 1000);
  mqtt_send(DSMR_P_IN_L1, msg, 0);

  /*  in Watts */
  sprintf(msg, "%.0f", data->P_in[1] * 1000);
  mqtt_send(DSMR_P_IN_L2, msg, 0);
  
  /*  in Watts */
  sprintf(msg, "%.0f", data->P_in[2] * 1000);
  mqtt_send(DSMR_P_IN_L3, msg, 0);


  /*  in kWh */
  sprintf(msg, "%.3f", data->E_in[1]);
  mqtt_send(DSMR_E_IN_TARIFF1, msg, 0);

  /*  in kWh */
  sprintf(msg, "%.3f", data->E_in[2]);
  mqtt_send(DSMR_E_IN_TARIFF2, msg, 0);

  /*  in kWh */
  sprintf(msg, "%.3f", data->E_out[1]);
  mqtt_send(DSMR_E_OUT_TARIFF1, msg, 0);

  /*  in kWh */
  sprintf(msg, "%.3f", data->E_out[2]);
  mqtt_send(DSMR_E_OUT_TARIFF2, msg, 0);
  

  // Gas values (with retain)
  if (last_gas_timestamp != data->dev_counter_timestamp[0]) {
    sprintf(msg, "%i", data->dev_counter_timestamp[0]);
    mqtt_send(DSMR_DEV_COUNTER_TIMESTAMP, msg, 1);
    
    sprintf(msg, "%.3f", data->dev_counter[0]);
    mqtt_send(DSMR_DEV_COUNTER, msg, 1);
    
    // Debiet is ((now gas - previous gas) / 60*60 (sec/hour) * (now timestamp - last timestamp))
    double debiet = ((data->dev_counter[0] - last_gas_count) / (60*60*(data->dev_counter_timestamp[0] - last_gas_timestamp)));
    sprintf(msg, "%.3f", debiet);
    mqtt_send(DSMR_DEV_COUNTER_RATE, msg, 1);
    
    last_gas_count     = data->dev_counter[0];
    last_gas_timestamp = data->dev_counter_timestamp[0];
  }
  
  free(msg);
  return 0;
}

int main(int argc, char **argv) {
  signal(SIGINT, intHandler);
  
  init_msglogger();
  // logger.loglevel = LL_VERBOSE;

  // set defaults in config
  config.serial_device = "/dev/ttyUSB0";
  config.mqtt_broker_host = "localhost";
  config.mqtt_broker_port = 1883;

  // parse arguments
  parse_arguments(argc, argv);

  // setup mqtt connection
  mqtt_setup(config.mqtt_broker_host, config.mqtt_broker_port);

  telegram_parser parser;
  telegram_parser_open(&parser, config.serial_device, 0, 0, NULL);
  telegram_parser_read(&parser);

  struct dsmr_data_struct *data = parser.data;

  int err;

  do {
    // TODO: figure out how to handle errors, time-outs, etc.
    telegram_parser_read(&parser);

    // Send values to MQTT broker
    send_values(data);

    // Call Mosquitto
    // err = mosquitto_loop(mosq, -1, 1);
    // fprintf(stderr, "MQTT loop return %i\n", err);

  } while (parser.terminal && keepRunning); // If we're connected to a 
                                            // serial device, keep 
                                            // reading, otherwise exit

  telegram_parser_close(&parser);

  mosquitto_destroy(mosq);
  mosquitto_lib_cleanup();

  return 0;
}
