/*
 * Copyright (c) 10/2018, RadioControlli srl
 * ver 1.0
 * All rights reserved.
 */
/**
 * \addtogroup RCCC1310-examples
 */
/*---------------------------------------------------------------------------*/
#ifndef CC26XX_WEB_DEMO_H_
#define CC26XX_WEB_DEMO_H_
/*---------------------------------------------------------------------------*/
#include "dev/leds.h"
#include "sys/process.h"
#include "mqtt-client.h"

#include <stdint.h>
/*---------------------------------------------------------------------------*/
#ifdef CC26XX_WEB_DEMO_CONF_MQTT_CLIENT
#define CC26XX_WEB_DEMO_MQTT_CLIENT CC26XX_WEB_DEMO_CONF_MQTT_CLIENT
#else
#define CC26XX_WEB_DEMO_MQTT_CLIENT 1
#endif

#ifdef CC26XX_WEB_DEMO_CONF_6LBR_CLIENT
#define CC26XX_WEB_DEMO_6LBR_CLIENT CC26XX_WEB_DEMO_CONF_6LBR_CLIENT
#else
#define CC26XX_WEB_DEMO_6LBR_CLIENT 1
#endif

#ifdef CC26XX_WEB_DEMO_CONF_COAP_SERVER
#define CC26XX_WEB_DEMO_COAP_SERVER CC26XX_WEB_DEMO_CONF_COAP_SERVER
#else
#define CC26XX_WEB_DEMO_COAP_SERVER 0
#endif

#ifdef CC26XX_WEB_DEMO_CONF_ADC_DEMO //process adc
#define CC26XX_WEB_DEMO_ADC_DEMO CC26XX_WEB_DEMO_CONF_ADC_DEMO
#else
#define CC26XX_WEB_DEMO_ADC_DEMO 0
#endif
/*---------------------------------------------------------------------------*/
/* Enable RSSI	 */
#if (CC26XX_WEB_DEMO_COAP_SERVER || CC26XX_WEB_DEMO_MQTT_CLIENT)
#define CC26XX_WEB_DEMO_READ_PARENT_RSSI 1
#else
#define CC26XX_WEB_DEMO_READ_PARENT_RSSI 0
#endif

#define CC26XX_WEB_DEMO_RSSI_MEASURE_INTERVAL_MAX 86400 /* secs: 1 day */
#define CC26XX_WEB_DEMO_RSSI_MEASURE_INTERVAL_MIN     5 /* secs */
/*---------------------------------------------------------------------------*/
/* Configuration user */
#define CC26XX_WEB_DEMO_SENSOR_READING_TRIGGER &button_left_sensor
#define CC26XX_WEB_DEMO_ECHO_REQ_PAYLOAD_LEN   20

#if DEVICE

#define CC26XX_WEB_DEMO_MQTT_PUBLISH_TRIGGER &reed_relay_sensor
#elif BOARD_LAUNCHPAD
#define CC26XX_WEB_DEMO_MQTT_PUBLISH_TRIGGER &button_left_sensor
#else
#define CC26XX_WEB_DEMO_MQTT_PUBLISH_TRIGGER &button_down_sensor
#endif 

#define CC26XX_WEB_DEMO_STATUS_LED LEDS_RED
/*---------------------------------------------------------------------------*/
/* Time of disconnect */
#define CC26XX_WEB_DEMO_NET_CONNECT_PERIODIC        (CLOCK_SECOND >> 3)
/*---------------------------------------------------------------------------*/
/* Default config of Cloud IBM */
#define CC26XX_WEB_DEMO_DEFAULT_ORG_ID              "8sizef" //change with your IBM ID
#if CPU_FAMILY_CC13XX
#define CC26XX_WEB_DEMO_DEFAULT_TYPE_ID             "cc13xx"
#else
#define CC26XX_WEB_DEMO_DEFAULT_TYPE_ID             "cc26xx"
#endif
#define CC26XX_WEB_DEMO_DEFAULT_EVENT_TYPE_ID       "status"
#define CC26XX_WEB_DEMO_DEFAULT_AUTH_TOKEN          "test0102"
#define CC26XX_WEB_DEMO_DEFAULT_SUBSCRIBE_CMD_TYPE  "+"
#define CC26XX_WEB_DEMO_DEFAULT_BROKER_PORT         1883
#define CC26XX_WEB_DEMO_DEFAULT_PUBLISH_INTERVAL    (20 * CLOCK_SECOND)
#define CC26XX_WEB_DEMO_DEFAULT_KEEP_ALIVE_TIMER    40
#define CC26XX_WEB_DEMO_DEFAULT_RSSI_MEAS_INTERVAL  (CLOCK_SECOND * 20)
/*---------------------------------------------------------------------------*/
/* Type Sensor */
#define CC26XX_WEB_DEMO_SENSOR_BATMON_TEMP   0
#define CC26XX_WEB_DEMO_SENSOR_BATMON_VOLT   1
#define CC26XX_WEB_DEMO_SENSOR_ADC_DIO23     15
/*---------------------------------------------------------------------------*/
extern process_event_t cc26xx_web_demo_publish_event;
extern process_event_t cc26xx_web_demo_config_loaded_event;
extern process_event_t cc26xx_web_demo_load_config_defaults;
/*---------------------------------------------------------------------------*/
#define CC26XX_WEB_DEMO_UNIT_TEMP     "C"
#define CC26XX_WEB_DEMO_UNIT_VOLT     "mV"
#define CC26XX_WEB_DEMO_UNIT_PRES     "hPa"
#define CC26XX_WEB_DEMO_UNIT_HUMIDITY "%RH"
#define CC26XX_WEB_DEMO_UNIT_LIGHT    "lux"
#define CC26XX_WEB_DEMO_UNIT_ACC      "G"
#define CC26XX_WEB_DEMO_UNIT_GYRO     "deg per sec"
/*---------------------------------------------------------------------------*/
#define CC26XX_WEB_DEMO_CONVERTED_LEN        12

typedef struct cc26xx_web_demo_sensor_reading {
  struct cc26xx_web_demo_sensor_reading *next;
  int raw;
  int last;
  const char *descr;
  const char *xml_element;
  const char *form_field;
  char *units;
  uint8_t type;
  uint8_t publish;
  uint8_t changed;
  char converted[CC26XX_WEB_DEMO_CONVERTED_LEN];
} cc26xx_web_demo_sensor_reading_t;
/*---------------------------------------------------------------------------*/
/* Global configuration */
typedef struct cc26xx_web_demo_config_s {
  uint32_t magic;
  int len;
  uint32_t sensors_bitmap;
  int def_rt_ping_interval;
  mqtt_client_config_t mqtt_config;
} cc26xx_web_demo_config_t;

extern cc26xx_web_demo_config_t cc26xx_web_demo_config;
/*---------------------------------------------------------------------------*/
const cc26xx_web_demo_sensor_reading_t *cc26xx_web_demo_sensor_lookup(int sens_type);

const cc26xx_web_demo_sensor_reading_t *cc26xx_web_demo_sensor_first(void);

int cc26xx_web_demo_ipaddr_sprintf(char *buf, uint8_t buf_len,
                                   const uip_ipaddr_t *addr);

void cc26xx_web_demo_restore_defaults(void);
/*---------------------------------------------------------------------------*/
#endif /* CC26XX_WEB_DEMO_H_ */
/*---------------------------------------------------------------------------*/
/**
 * @}
 * @}
 */

