/*
 * Copyright (c) 10/2018, RadioControlli srl.
 * ver 1.0
 * All rights reserved.
 *
/**
 * \principal code RCCC1310-demo
 * @{
 *
 */
/*---------------------------------------------------------------------------*/
#include "contiki.h"
#include "contiki-net.h"
#include "rest-engine.h"
#include "board-peripherals.h"
#include "lib/sensors.h"
#include "lib/list.h"
#include "sys/process.h"
#include "net/ipv6/sicslowpan.h"
#include "button-sensor.h"
#include "batmon-sensor.h"
#include "httpd-simple.h"
#include "RC-CC1310-868-demo.h"
#include "mqtt-client.h"
#include "coap-server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ti-lib.h"
/*---------------------------------------------------------------------------*/
PROCESS_NAME(cetic_6lbr_client_process);
PROCESS(cc26xx_web_demo_process, "CC26XX Web Demo");
/*---------------------------------------------------------------------------*/
/*
 * Read sensor time
 *
 */
#define SENSOR_READING_PERIOD (CLOCK_SECOND * 20)
#define SENSOR_READING_RANDOM (CLOCK_SECOND << 4)

struct ctimer batmon_timer;
/*---------------------------------------------------------------------------*/
/* Led blink  */ 
#define NO_NET_LED_DURATION        (CC26XX_WEB_DEMO_NET_CONNECT_PERIODIC >> 1)

static struct etimer et;
static struct ctimer ct;
/*---------------------------------------------------------------------------*/
/* RSSI  */
#if CC26XX_WEB_DEMO_READ_PARENT_RSSI
static struct uip_icmp6_echo_reply_notification echo_reply_notification;
static struct etimer echo_request_timer;
int def_rt_rssi = 0;
#endif
/*---------------------------------------------------------------------------*/
#if CC26XX_WEB_DEMO_ADC_DEMO
PROCESS(adc_process, "ADC process");

static uint16_t single_adc_sample;
static struct etimer et_adc;
#endif
/*---------------------------------------------------------------------------*/
process_event_t cc26xx_web_demo_publish_event;
process_event_t cc26xx_web_demo_config_loaded_event;
process_event_t cc26xx_web_demo_load_config_defaults;
/*---------------------------------------------------------------------------*/
/* Storage date */
#define CONFIG_FLASH_OFFSET        0
#define CONFIG_MAGIC      0xCC265002

cc26xx_web_demo_config_t cc26xx_web_demo_config;
/*---------------------------------------------------------------------------*/
LIST(sensor_list);
/*---------------------------------------------------------------------------*/
/* Initial sensor */
#define DEMO_SENSOR(name, type, descr, xml_element, form_field, units) \
  cc26xx_web_demo_sensor_reading_t name##_reading = \
  { NULL, 0, 0, descr, xml_element, form_field, units, type, 1, 1 }

/* RCCC1310 sensors */
DEMO_SENSOR(batmon_temp, CC26XX_WEB_DEMO_SENSOR_BATMON_TEMP,
            "TemperaturaBatt", "battery-temp", "batmon_temp",
           CC26XX_WEB_DEMO_UNIT_TEMP);
DEMO_SENSOR(batmon_volt, CC26XX_WEB_DEMO_SENSOR_BATMON_VOLT,
            "BatteryVolt", "battery-volt", "batmon_volt",
           CC26XX_WEB_DEMO_UNIT_VOLT);

#if CC26XX_WEB_DEMO_ADC_DEMO
DEMO_SENSOR(adc_dio23, CC26XX_WEB_DEMO_SENSOR_ADC_DIO23,
            "ADC DIO23", "adc-dio23", "adc_dio23",
            CC26XX_WEB_DEMO_UNIT_VOLT);
#endif
/*---------------------------------------------------------------------------*/
static void
publish_led_off(void *d)
{
  leds_off(CC26XX_WEB_DEMO_STATUS_LED);
}
/*---------------------------------------------------------------------------*/
static void
save_config()
{
#if BOARD_SENSORTAG || BOARD_LAUNCHPAD
  int rv;
  cc26xx_web_demo_sensor_reading_t *reading = NULL;

  rv = ext_flash_open();

  if(!rv) {
    printf("Could not open flash to save config\n");
    ext_flash_close();
    return;
  }

  rv = ext_flash_erase(CONFIG_FLASH_OFFSET, sizeof(cc26xx_web_demo_config_t));

  if(!rv) {
    printf("Error erasing flash\n");
  } else {
    cc26xx_web_demo_config.magic = CONFIG_MAGIC;
    cc26xx_web_demo_config.len = sizeof(cc26xx_web_demo_config_t);
    cc26xx_web_demo_config.sensors_bitmap = 0;

    for(reading = list_head(sensor_list);
        reading != NULL;
        reading = list_item_next(reading)) {
      if(reading->publish) {
        cc26xx_web_demo_config.sensors_bitmap |= (1 << reading->type);
      }
    }

    rv = ext_flash_write(CONFIG_FLASH_OFFSET, sizeof(cc26xx_web_demo_config_t),
                         (uint8_t *)&cc26xx_web_demo_config);
    if(!rv) {
      printf("Error saving config\n");
    }
  }

  ext_flash_close();
#endif
}
/*---------------------------------------------------------------------------*/
static void
load_config()
{
#if BOARD_SENSORTAG || BOARD_LAUNCHPAD
  /* Read flash  */
  cc26xx_web_demo_config_t tmp_cfg;
  cc26xx_web_demo_sensor_reading_t *reading = NULL;

  int rv = ext_flash_open();

  if(!rv) {
    printf("Could not open flash to load config\n");
    ext_flash_close();
    return;
  }

  rv = ext_flash_read(CONFIG_FLASH_OFFSET, sizeof(tmp_cfg),
                      (uint8_t *)&tmp_cfg);

  ext_flash_close();

  if(!rv) {
    printf("Error loading config\n");
    return;
  }

  if(tmp_cfg.magic == CONFIG_MAGIC && tmp_cfg.len == sizeof(tmp_cfg)) {
    memcpy(&cc26xx_web_demo_config, &tmp_cfg, sizeof(cc26xx_web_demo_config));
  }

  for(reading = list_head(sensor_list);
      reading != NULL;
      reading = list_item_next(reading)) {
    if(cc26xx_web_demo_config.sensors_bitmap & (1 << reading->type)) {
      reading->publish = 1;
    } else {
      reading->publish = 0;
      snprintf(reading->converted, CC26XX_WEB_DEMO_CONVERTED_LEN, "\"N/A\"");
    }
  }
#endif
}
/*---------------------------------------------------------------------------*/
AUTOSTART_PROCESSES(&cc26xx_web_demo_process);
/*---------------------------------------------------------------------------*/
int
cc26xx_web_demo_ipaddr_sprintf(char *buf, uint8_t buf_len,
                               const uip_ipaddr_t *addr)
{
  uint16_t a;
  uint8_t len = 0;
  int i, f;
  for(i = 0, f = 0; i < sizeof(uip_ipaddr_t); i += 2) {
    a = (addr->u8[i] << 8) + addr->u8[i + 1];
    if(a == 0 && f >= 0) {
      if(f++ == 0) {
        len += snprintf(&buf[len], buf_len - len, "::");
      }
    } else {
      if(f > 0) {
        f = -1;
      } else if(i > 0) {
        len += snprintf(&buf[len], buf_len - len, ":");
      }
      len += snprintf(&buf[len], buf_len - len, "%x", a);
    }
  }

  return len;
}
/*---------------------------------------------------------------------------*/
const cc26xx_web_demo_sensor_reading_t *
cc26xx_web_demo_sensor_lookup(int sens_type)
{
  cc26xx_web_demo_sensor_reading_t *reading = NULL;

  for(reading = list_head(sensor_list);
      reading != NULL;
      reading = list_item_next(reading)) {
    if(reading->type == sens_type) {
      return reading;
    }
  }

  return NULL;
}
/*---------------------------------------------------------------------------*/
const cc26xx_web_demo_sensor_reading_t *
cc26xx_web_demo_sensor_first()
{
  return list_head(sensor_list);
}
/*---------------------------------------------------------------------------*/
void
cc26xx_web_demo_restore_defaults(void)
{
  cc26xx_web_demo_sensor_reading_t *reading = NULL;

  leds_on(LEDS_ALL);

  for(reading = list_head(sensor_list);
      reading != NULL;
      reading = list_item_next(reading)) {
    reading->publish = 1;
  }

#if CC26XX_WEB_DEMO_MQTT_CLIENT
  process_post_synch(&mqtt_client_process,
                     cc26xx_web_demo_load_config_defaults, NULL);
#endif

  save_config();

  leds_off(LEDS_ALL);
}
/*---------------------------------------------------------------------------*/
static int
defaults_post_handler(char *key, int key_len, char *val, int val_len)
{
  if(key_len != strlen("defaults") ||
     strncasecmp(key, "defaults", strlen("defaults")) != 0) {
  
    return HTTPD_SIMPLE_POST_HANDLER_UNKNOWN;
  }

  cc26xx_web_demo_restore_defaults();

  return HTTPD_SIMPLE_POST_HANDLER_OK;
}
/*---------------------------------------------------------------------------*/
static int
sensor_readings_handler(char *key, int key_len, char *val, int val_len)
{
  cc26xx_web_demo_sensor_reading_t *reading = NULL;
  int rv;

  for(reading = list_head(sensor_list);
      reading != NULL;
      reading = list_item_next(reading)) {
    if(key_len == strlen(reading->form_field) &&
       strncmp(reading->form_field, key, strlen(key)) == 0) {

      rv = atoi(val);

      if(rv == 0) {
        reading->publish = 0;
        snprintf(reading->converted, CC26XX_WEB_DEMO_CONVERTED_LEN, "\"N/A\"");
      } else if(rv == 1) {
        reading->publish = 1;
      } else {
        return HTTPD_SIMPLE_POST_HANDLER_ERROR;
      }

      return HTTPD_SIMPLE_POST_HANDLER_OK;
    }
  }

  return HTTPD_SIMPLE_POST_HANDLER_UNKNOWN;
}
/*---------------------------------------------------------------------------*/
#if CC26XX_WEB_DEMO_READ_PARENT_RSSI
static int
ping_interval_post_handler(char *key, int key_len, char *val, int val_len)
{
  int rv = 0;

  if(key_len != strlen("ping_interval") ||
     strncasecmp(key, "ping_interval", strlen("ping_interval")) != 0) {
    /* Not ours */
    return HTTPD_SIMPLE_POST_HANDLER_UNKNOWN;
  }

  rv = atoi(val);

  if(rv < CC26XX_WEB_DEMO_RSSI_MEASURE_INTERVAL_MIN ||
     rv > CC26XX_WEB_DEMO_RSSI_MEASURE_INTERVAL_MAX) {
    return HTTPD_SIMPLE_POST_HANDLER_ERROR;
  }

  cc26xx_web_demo_config.def_rt_ping_interval = rv * CLOCK_SECOND;

  return HTTPD_SIMPLE_POST_HANDLER_OK;
}
#endif
/*---------------------------------------------------------------------------*/
HTTPD_SIMPLE_POST_HANDLER(sensor, sensor_readings_handler);
HTTPD_SIMPLE_POST_HANDLER(defaults, defaults_post_handler);

#if CC26XX_WEB_DEMO_READ_PARENT_RSSI
HTTPD_SIMPLE_POST_HANDLER(ping_interval, ping_interval_post_handler);
/*---------------------------------------------------------------------------*/
static void
echo_reply_handler(uip_ipaddr_t *source, uint8_t ttl, uint8_t *data,
                   uint16_t datalen)
{
  if(uip_ip6addr_cmp(source, uip_ds6_defrt_choose())) {
    def_rt_rssi = sicslowpan_get_last_rssi();
  }
}
/*---------------------------------------------------------------------------*/
static void
ping_parent(void)
{
  if(uip_ds6_get_global(ADDR_PREFERRED) == NULL) {
    return;
  }

  uip_icmp6_send(uip_ds6_defrt_choose(), ICMP6_ECHO_REQUEST, 0,
                 CC26XX_WEB_DEMO_ECHO_REQ_PAYLOAD_LEN);
}
#endif
/*---------------------------------------------------------------------------*/
static void
get_batmon_reading(void *data)
{
  int value;
  char *buf;
  clock_time_t next = SENSOR_READING_PERIOD +
    (random_rand() % SENSOR_READING_RANDOM);

  if(batmon_temp_reading.publish) {
    value = batmon_sensor.value(BATMON_SENSOR_TYPE_TEMP);
    if(value != CC26XX_SENSOR_READING_ERROR) {
      batmon_temp_reading.raw = value;

      buf = batmon_temp_reading.converted;
      memset(buf, 0, CC26XX_WEB_DEMO_CONVERTED_LEN);
      snprintf(buf, CC26XX_WEB_DEMO_CONVERTED_LEN, "%d", value);
    }
  }

  if(batmon_volt_reading.publish) {
    value = batmon_sensor.value(BATMON_SENSOR_TYPE_VOLT);
    if(value != CC26XX_SENSOR_READING_ERROR) {
      batmon_volt_reading.raw = value;

      buf = batmon_volt_reading.converted;
      memset(buf, 0, CC26XX_WEB_DEMO_CONVERTED_LEN);
      snprintf(buf, CC26XX_WEB_DEMO_CONVERTED_LEN, "%d", (value * 125) >> 5);
    }
  }

  ctimer_set(&batmon_timer, next, get_batmon_reading, NULL);
}
/*---------------------------------------------------------------------------*/
#if CC26XX_WEB_DEMO_ADC_DEMO
static void
get_adc_reading(void *data)
{
  int value;
  char *buf;

  if(adc_dio23_reading.publish) {
    value = single_adc_sample;
    buf = adc_dio23_reading.converted;
    memset(buf, 0, CC26XX_WEB_DEMO_CONVERTED_LEN);
    snprintf(buf, CC26XX_WEB_DEMO_CONVERTED_LEN, "%d", (value * 4300) >> 12);
  }
}
#endif
/*---------------------------------------------------------------------------*/
static void
init_sensor_readings(void)
{
  /*
   * Reading sensor
   */
  get_batmon_reading(NULL);

  return;
}
/*---------------------------------------------------------------------------*/
static void
init_sensors(void)
{

  list_add(sensor_list, &batmon_temp_reading);
  list_add(sensor_list, &batmon_volt_reading);

#if CC26XX_WEB_DEMO_ADC_DEMO
  list_add(sensor_list, &adc_dio23_reading);
#endif

  SENSORS_ACTIVATE(batmon_sensor);
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(cc26xx_web_demo_process, ev, data)
{
  PROCESS_BEGIN();

  printf("CC26XX Web Demo Process\n");

  init_sensors();

  cc26xx_web_demo_publish_event = process_alloc_event();
  cc26xx_web_demo_config_loaded_event = process_alloc_event();
  cc26xx_web_demo_load_config_defaults = process_alloc_event();

  /* Start process */
  process_start(&httpd_simple_process, NULL);

#if CC26XX_WEB_DEMO_COAP_SERVER
  process_start(&coap_server_process, NULL);
#endif

#if CC26XX_WEB_DEMO_6LBR_CLIENT
  process_start(&cetic_6lbr_client_process, NULL);
#endif

#if CC26XX_WEB_DEMO_MQTT_CLIENT
  process_start(&mqtt_client_process, NULL);
#endif

#if CC26XX_WEB_DEMO_ADC_DEMO
  process_start(&adc_process, NULL);
#endif

  
  cc26xx_web_demo_config.sensors_bitmap = 0xFFFFFFFF; /* all on by default */
  cc26xx_web_demo_config.def_rt_ping_interval =
    CC26XX_WEB_DEMO_DEFAULT_RSSI_MEAS_INTERVAL;
  load_config();

  process_post(PROCESS_BROADCAST, cc26xx_web_demo_config_loaded_event, NULL);

  init_sensor_readings();

  httpd_simple_register_post_handler(&sensor_handler);
  httpd_simple_register_post_handler(&defaults_handler);

#if CC26XX_WEB_DEMO_READ_PARENT_RSSI
  httpd_simple_register_post_handler(&ping_interval_handler);

  def_rt_rssi = 0x8000000;
  uip_icmp6_echo_reply_callback_add(&echo_reply_notification,
                                    echo_reply_handler);
  etimer_set(&echo_request_timer, CC26XX_WEB_DEMO_NET_CONNECT_PERIODIC);
#endif

  etimer_set(&et, CC26XX_WEB_DEMO_NET_CONNECT_PERIODIC);

  while(1) {
    if(ev == PROCESS_EVENT_TIMER && etimer_expired(&et)) {
      if(uip_ds6_get_global(ADDR_PREFERRED) == NULL) {
        leds_on(CC26XX_WEB_DEMO_STATUS_LED);
        ctimer_set(&ct, NO_NET_LED_DURATION, publish_led_off, NULL);
        etimer_set(&et, CC26XX_WEB_DEMO_NET_CONNECT_PERIODIC);
      }
    }

#if CC26XX_WEB_DEMO_READ_PARENT_RSSI
    if(ev == PROCESS_EVENT_TIMER && etimer_expired(&echo_request_timer)) {
      if(uip_ds6_get_global(ADDR_PREFERRED) == NULL) {
        etimer_set(&echo_request_timer, CC26XX_WEB_DEMO_NET_CONNECT_PERIODIC);
      } else {
        ping_parent();
        etimer_set(&echo_request_timer, cc26xx_web_demo_config.def_rt_ping_interval);
      }
    }
#endif

    if(ev == sensors_event && data == CC26XX_WEB_DEMO_SENSOR_READING_TRIGGER) {
      if((CC26XX_WEB_DEMO_SENSOR_READING_TRIGGER)->value(
           BUTTON_SENSOR_VALUE_DURATION) > CLOCK_SECOND * 5) {
        printf("Restoring defaults!\n");
        cc26xx_web_demo_restore_defaults();
      } else {
        init_sensor_readings();

        process_post(PROCESS_BROADCAST, cc26xx_web_demo_publish_event, NULL);
      }
    } else if(ev == httpd_simple_event_new_config) {
      save_config();
    }

    PROCESS_YIELD();
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
#if CC26XX_WEB_DEMO_ADC_DEMO
PROCESS_THREAD(adc_process, ev, data)
{
  PROCESS_BEGIN();

  etimer_set(&et_adc, CLOCK_SECOND * 5);

  while(1) {

    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et_adc));

    /* Start ADC */
    ti_lib_aon_wuc_aux_wakeup_event(AONWUC_AUX_WAKEUP);
    while(!(ti_lib_aon_wuc_power_status_get() & AONWUC_AUX_POWER_ON));

    /*
     * Enable Clock
     */
    ti_lib_aux_wuc_clock_enable(AUX_WUC_ADI_CLOCK | AUX_WUC_ANAIF_CLOCK |
                                AUX_WUC_SMPH_CLOCK);
    while(ti_lib_aux_wuc_clock_status(AUX_WUC_ADI_CLOCK | AUX_WUC_ANAIF_CLOCK |
                                      AUX_WUC_SMPH_CLOCK)
          != AUX_WUC_CLOCK_READY);

    /* Connect AUX IO7 (DIO23) such as Input Analogic. */
    ti_lib_aux_adc_select_input(ADC_COMPB_IN_AUXIO7);

    /* range ADC 4.3V*/
    ti_lib_aux_adc_enable_sync(AUXADC_REF_FIXED, AUXADC_SAMPLE_TIME_2P7_US,
                               AUXADC_TRIGGER_MANUAL);

    /* Conversion */
    ti_lib_aux_adc_gen_manual_trigger();

    /* Read value */
    single_adc_sample = ti_lib_aux_adc_read_fifo();

    /* Close ADC */
    ti_lib_aux_adc_disable();

    get_adc_reading(NULL);

    etimer_reset(&et_adc);
  }

  PROCESS_END();
}
#endif
/*---------------------------------------------------------------------------*/
/**
 * @}
 */

