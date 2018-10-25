/*
 * Copyright (c) 10/2018, RadioControlli SRL -
 * ver 1.0
 * All rights reserved.
 */
 /*
 *
 * \file
 *   Connect to broker IBM Cloud.
 */
/*---------------------------------------------------------------------------*/
#include "contiki-conf.h"
#include "rpl/rpl-private.h"
#include "mqtt.h"
#include "net/rpl/rpl.h"
#include "net/ip/uip.h"
#include "net/ipv6/uip-icmp6.h"
#include "sys/etimer.h"
#include "sys/ctimer.h"
#include "lib/sensors.h"
#include "button-sensor.h"
#include "board-peripherals.h"
#include "RC-CC1310-868-demo.h"
#include "dev/leds.h"
#include "mqtt-client.h"
#include "httpd-simple.h"

#include <string.h>
#include <strings.h>
/*---------------------------------------------------------------------------*/
/*
 * IBM server: messaging.quickstart.internetofthings.ibmcloud.com
 * (prefix 64:ff9b::/96) Prefisso se vuoi usare Rasp/slip-radio
 *
 **********************************************************************************/
static const char *broker_ip = "0000:0000:0000:0000:0000:ffff:9f08:a9d4";     //broker ip Gateway Weptech 
/*---------------------------------------------------------------------------*/
#define ADDRESS_CONVERSION_OK       1
#define ADDRESS_CONVERSION_ERROR    0
/*---------------------------------------------------------------------------*/
 /*
  * Public test led on/off
  *
  */
#define PUBLIC_TOPIC "iot-2/evt/%s/fmt/json"
#define SUB_TOPIC "iot-2/cmd/%s/fmt/json"
/*---------------------------------------------------------------------------*/
/*
 * Time of Disconnecting
 */
#define STATE_MACHINE_PERIODIC     (CLOCK_SECOND >> 1)
/*---------------------------------------------------------------------------*/
/* Clock Time led */
#define CONNECTING_LED_DURATION    (CLOCK_SECOND >> 3)

/* Public time */
#define PUBLISH_LED_ON_DURATION    (CLOCK_SECOND)
/*---------------------------------------------------------------------------*/
#define RETRY_FOREVER              0xFF
#define RECONNECT_INTERVAL         (CLOCK_SECOND * 2)

#define RECONNECT_ATTEMPTS         5
#define CONNECTION_STABLE_TIME     (CLOCK_SECOND * 5)
#define NEW_CONFIG_WAIT_INTERVAL   (CLOCK_SECOND * 20)
static struct timer connection_life;
static uint8_t connect_attempt;
/*---------------------------------------------------------------------------*/
/* Description state */
static uint8_t state;
#define MQTT_CLIENT_STATE_INIT            0
#define MQTT_CLIENT_STATE_REGISTERED      1
#define MQTT_CLIENT_STATE_CONNECTING      2
#define MQTT_CLIENT_STATE_CONNECTED       3
#define MQTT_CLIENT_STATE_PUBLISHING      4
#define MQTT_CLIENT_STATE_DISCONNECTED    5
#define MQTT_CLIENT_STATE_NEWCONFIG       6
#define MQTT_CLIENT_STATE_CONFIG_ERROR 0xFE
#define MQTT_CLIENT_STATE_ERROR        0xFF
/*---------------------------------------------------------------------------*/
#define MQTT_CLIENT_MAX_SEGMENT_SIZE    32
/*---------------------------------------------------------------------------*/
#define BUFFER_SIZE 64
static char client_id[BUFFER_SIZE];
static char pub_topic[BUFFER_SIZE];
static char sub_topic[BUFFER_SIZE];
/*---------------------------------------------------------------------------*/
#define APP_BUFFER_SIZE 512
static struct mqtt_connection conn;
static char app_buffer[APP_BUFFER_SIZE];
/*---------------------------------------------------------------------------*/
#define QUICKSTART "quickstart"  
/*---------------------------------------------------------------------------*/
static struct mqtt_message *msg_ptr = 0;
static struct etimer publish_periodic_timer;
static struct ctimer ct;
static char *buf_ptr;
static uint16_t seq_nr_value = 0;
/*---------------------------------------------------------------------------*/
static uip_ip6addr_t def_route;
/*---------------------------------------------------------------------------*/
/* Parent RSSI functionality */
extern int def_rt_rssi;
/*---------------------------------------------------------------------------*/
const static cc26xx_web_demo_sensor_reading_t *reading;
/*---------------------------------------------------------------------------*/
mqtt_client_config_t *conf;
/*---------------------------------------------------------------------------*/
PROCESS(mqtt_client_process, "CC26XX MQTT Client");
/*---------------------------------------------------------------------------*/
static void  //Initial led
publish_led_off(void *d)
{
  leds_off(CC26XX_WEB_DEMO_STATUS_LED);
}
/*---------------------------------------------------------------------------*/
static void
new_net_config(void)
{
  /*
   * New configuration
   */
  if(state == MQTT_CLIENT_STATE_NEWCONFIG) {
    return;
  }

  state = MQTT_CLIENT_STATE_NEWCONFIG;

  etimer_stop(&publish_periodic_timer);
  mqtt_disconnect(&conn);
}
/*---------------------------------------------------------------------------*/
static int //initial org-id
org_id_post_handler(char *key, int key_len, char *val, int val_len)
{
  int rv = HTTPD_SIMPLE_POST_HANDLER_UNKNOWN;
  if(key_len != strlen("org_id") ||
     strncasecmp(key, "org_id", strlen("org_id")) != 0) {
 
    return HTTPD_SIMPLE_POST_HANDLER_UNKNOWN;
  }

  if(val_len > MQTT_CLIENT_CONFIG_ORG_ID_LEN) {
  
    rv = HTTPD_SIMPLE_POST_HANDLER_ERROR;
  } else {
    memset(conf->org_id, 0, MQTT_CLIENT_CONFIG_ORG_ID_LEN);
    memcpy(conf->org_id, val, val_len);

    rv = HTTPD_SIMPLE_POST_HANDLER_OK;
  }

  new_net_config();

  return rv;
}
/*---------------------------------------------------------------------------*/
static int //initial type id
type_id_post_handler(char *key, int key_len, char *val, int val_len)
{
  int rv = HTTPD_SIMPLE_POST_HANDLER_UNKNOWN;
  if(key_len != strlen("type_id") ||
     strncasecmp(key, "type_id", strlen("type_id")) != 0) {
    
    return HTTPD_SIMPLE_POST_HANDLER_UNKNOWN;
  }

  if(val_len > MQTT_CLIENT_CONFIG_TYPE_ID_LEN) {
 
    rv = HTTPD_SIMPLE_POST_HANDLER_ERROR;
  } else {
    memset(conf->type_id, 0, MQTT_CLIENT_CONFIG_TYPE_ID_LEN);
    memcpy(conf->type_id, val, val_len);

    rv = HTTPD_SIMPLE_POST_HANDLER_OK;
  }

  new_net_config();

  return rv;
}
/*---------------------------------------------------------------------------*/
static int //initial event 
event_type_id_post_handler(char *key, int key_len, char *val, int val_len)
{
  int rv = HTTPD_SIMPLE_POST_HANDLER_UNKNOWN;
  if(key_len != strlen("event_type_id") ||
     strncasecmp(key, "event_type_id", strlen("event_type_id")) != 0) {
    
    return HTTPD_SIMPLE_POST_HANDLER_UNKNOWN;
  }

  if(val_len > MQTT_CLIENT_CONFIG_EVENT_TYPE_ID_LEN) {
    
    rv = HTTPD_SIMPLE_POST_HANDLER_ERROR;
  } else {
    memset(conf->event_type_id, 0, MQTT_CLIENT_CONFIG_EVENT_TYPE_ID_LEN);
    memcpy(conf->event_type_id, val, val_len);

    rv = HTTPD_SIMPLE_POST_HANDLER_OK;
  }

  new_net_config();

  return rv;
}
/*---------------------------------------------------------------------------*/
static int //type of command
cmd_type_post_handler(char *key, int key_len, char *val, int val_len)
{
  int rv = HTTPD_SIMPLE_POST_HANDLER_UNKNOWN;
  if(key_len != strlen("cmd_type") ||
     strncasecmp(key, "cmd_type", strlen("cmd_type")) != 0) {
  
    return HTTPD_SIMPLE_POST_HANDLER_UNKNOWN;
  }

  if(val_len > MQTT_CLIENT_CONFIG_CMD_TYPE_LEN) {
  
    rv = HTTPD_SIMPLE_POST_HANDLER_ERROR;
  } else {
    memset(conf->cmd_type, 0, MQTT_CLIENT_CONFIG_CMD_TYPE_LEN);
    memcpy(conf->cmd_type, val, val_len);

    rv = HTTPD_SIMPLE_POST_HANDLER_OK;
  }

  new_net_config();

  return rv;
}
/*---------------------------------------------------------------------------*/
static int //auth-token 
auth_token_post_handler(char *key, int key_len, char *val, int val_len)
{
  int rv = HTTPD_SIMPLE_POST_HANDLER_UNKNOWN;
  if(key_len != strlen("auth_token") || //auth_token
     strncasecmp(key, "auth_token", strlen("auth_token")) != 0) {
 
    return HTTPD_SIMPLE_POST_HANDLER_UNKNOWN;
  }

  if(val_len > MQTT_CLIENT_CONFIG_AUTH_TOKEN_LEN) {
 
    rv = HTTPD_SIMPLE_POST_HANDLER_ERROR;
  } else {
    memset(conf->auth_token, 0, MQTT_CLIENT_CONFIG_AUTH_TOKEN_LEN);
    memcpy(conf->auth_token, val, val_len);

    rv = HTTPD_SIMPLE_POST_HANDLER_OK;
  }

  printf("auth-token: %s", conf->auth_token); 

  new_net_config();

  return rv;
}
/*---------------------------------------------------------------------------*/
static int //Transmission time to the cloud
interval_post_handler(char *key, int key_len, char *val, int val_len)
{
  int rv = 0;

  if(key_len != strlen("interval") ||
     strncasecmp(key, "interval", strlen("interval")) != 0) {
   
    return HTTPD_SIMPLE_POST_HANDLER_UNKNOWN;
  }

  rv = atoi(val);

  if(rv < MQTT_CLIENT_PUBLISH_INTERVAL_MIN ||
     rv > MQTT_CLIENT_PUBLISH_INTERVAL_MAX) {
    return HTTPD_SIMPLE_POST_HANDLER_ERROR;
  }

  conf->pub_interval = rv * CLOCK_SECOND;

  return HTTPD_SIMPLE_POST_HANDLER_OK;
}
/*---------------------------------------------------------------------------*/
static int //Port ID and broker 
port_post_handler(char *key, int key_len, char *val, int val_len)
{
  int rv = 0;

  if(key_len != strlen("broker_port") ||
     strncasecmp(key, "broker_port", strlen("broker_port")) != 0) {
   
    return HTTPD_SIMPLE_POST_HANDLER_UNKNOWN;
  }

  rv = atoi(val);

  if(rv <= 65535 && rv > 0) {
    conf->broker_port = rv;
  } else {
    return HTTPD_SIMPLE_POST_HANDLER_ERROR;
  }

  new_net_config();

  return HTTPD_SIMPLE_POST_HANDLER_OK;
}
/*---------------------------------------------------------------------------*/
static int 
ip_addr_post_handler(char *key, int key_len, char *val, int val_len)
{
  int rv = HTTPD_SIMPLE_POST_HANDLER_UNKNOWN;

  uip_ip6addr_t tmp_addr;

  if(key_len != strlen("broker_ip") ||
     strncasecmp(key, "broker_ip", strlen("broker_ip")) != 0) {
  
    return HTTPD_SIMPLE_POST_HANDLER_UNKNOWN;
  }

  if(val_len > MQTT_CLIENT_CONFIG_IP_ADDR_STR_LEN
          || uiplib_ip6addrconv(val, &tmp_addr) != ADDRESS_CONVERSION_OK) {
    rv = HTTPD_SIMPLE_POST_HANDLER_ERROR;
  } else {
    memset(conf->broker_ip, 0, MQTT_CLIENT_CONFIG_IP_ADDR_STR_LEN);
    memcpy(conf->broker_ip, val, val_len);

    rv = HTTPD_SIMPLE_POST_HANDLER_OK;
  }

  new_net_config();

  return rv;
}
/*---------------------------------------------------------------------------*/
static int
reconnect_post_handler(char *key, int key_len, char *val, int val_len)
{
  if(key_len != strlen("reconnect") ||
     strncasecmp(key, "reconnect", strlen("reconnect")) != 0) {
    return HTTPD_SIMPLE_POST_HANDLER_UNKNOWN;
  }

  new_net_config();

  return HTTPD_SIMPLE_POST_HANDLER_OK;
}
/*---------------------------------------------------------------------------*/
HTTPD_SIMPLE_POST_HANDLER(org_id, org_id_post_handler);
HTTPD_SIMPLE_POST_HANDLER(type_id, type_id_post_handler);
HTTPD_SIMPLE_POST_HANDLER(event_type_id, event_type_id_post_handler);
HTTPD_SIMPLE_POST_HANDLER(cmd_type, cmd_type_post_handler);
HTTPD_SIMPLE_POST_HANDLER(auth_token, auth_token_post_handler);
HTTPD_SIMPLE_POST_HANDLER(ip_addr, ip_addr_post_handler);
HTTPD_SIMPLE_POST_HANDLER(port, port_post_handler);
HTTPD_SIMPLE_POST_HANDLER(interval, interval_post_handler);
HTTPD_SIMPLE_POST_HANDLER(reconnect, reconnect_post_handler);
/*---------------------------------------------------------------------------*/
static void //ON-OFF LED through node-red 

pub_handler(const char *topic, uint16_t topic_len, const uint8_t *chunk,
            uint16_t chunk_len)
{

   DBG("Pub Handler: topic='%s' (len=%u), chunk_len=%u\n", topic, topic_len,
      chunk_len);

  if(topic_len != 23 || chunk_len != 20) {
    printf("Incorrect topic or chunk len. Ignored\n");
    return;
  }

  /* Ignore JSON format */
  if(strncmp(&topic[topic_len - 4], "json", 4) != 0) {
    printf("Incorrect format\n");
  }

 if(strncmp(&topic[20], "open", 4)) {
          printf("aAPRIz");
          leds_on(LEDS_GREEN);
}
return; 
}
/*---------------------------------------------------------------------------*/
static void //Connecting and disconnecting 
mqtt_event(struct mqtt_connection *m, mqtt_event_t event, void *data)
{
  switch(event) {
  case MQTT_EVENT_CONNECTED: {
    DBG("APP - Application has a MQTT connection\n");
    timer_set(&connection_life, CONNECTION_STABLE_TIME);
    state = MQTT_CLIENT_STATE_CONNECTED;
    break;
  }
  case MQTT_EVENT_DISCONNECTED: {
    DBG("APP - MQTT Disconnect. Reason %u\n", *((mqtt_event_t *)data));

    if(state != MQTT_CLIENT_STATE_NEWCONFIG) {
      state = MQTT_CLIENT_STATE_DISCONNECTED;
      process_poll(&mqtt_client_process);
    }
    break;
  }
  case MQTT_EVENT_PUBLISH: {        /*----------NOTE THIS---------*/
    msg_ptr = data;

   
    if(msg_ptr->first_chunk) {
      msg_ptr->first_chunk = 0;
      DBG("APP - Application received a publish on topic '%s'. Payload "
          "size is %i bytes. Content:\n\n",
          msg_ptr->topic, msg_ptr->payload_length);
    }

    pub_handler(msg_ptr->topic, strlen(msg_ptr->topic), msg_ptr->payload_chunk,
                msg_ptr->payload_length);
    break;
  }
  case MQTT_EVENT_SUBACK: { /*---------SUBSCRIBEE A TOPIC-----*/
    DBG("APP - Application is subscribed to topic successfully\n");
    break;
  }
  case MQTT_EVENT_UNSUBACK: {   /*--------NOT SUBSCRIBE A TOPIC-------*/
    DBG("APP - Application is unsubscribed to topic successfully\n");
    break;
  }
  case MQTT_EVENT_PUBACK: {   /*--------PUBBLIC COMPLETE-------*/
    DBG("APP - Publishing complete.\n");
    break;
  }
  default:      /*-------------EVENT OK-----------*/
    DBG("APP - Application got a unhandled MQTT event: %i\n", event);
    break;
  }
}
/*---------------------------------------------------------------------------*/
static int //receive the message
construct_pub_topic(void)
{
  int len = snprintf(pub_topic, BUFFER_SIZE, PUBLIC_TOPIC,
                     conf->event_type_id);

  /* len < 0: Error. Len >= BUFFER_SIZE: Buffer too small */
  if(len < 0 || len >= BUFFER_SIZE) {
    printf("Pub Topic: %d, Buffer %d\n", len, BUFFER_SIZE);
    return 0;
  }

  return 1;
}
/*---------------------------------------------------------------------------*/
static int //describe a command that send
construct_sub_topic(void)
{
  int len = snprintf(sub_topic, BUFFER_SIZE, SUB_TOPIC,
                     conf->cmd_type);

  /* len < 0: Error. Len >= BUFFER_SIZE: Buffer too small */
  if(len < 0 || len >= BUFFER_SIZE) {
    printf("Sub Topic: %d, Buffer %d\n", len, BUFFER_SIZE);
    return 0;
  }

  return 1;
}
/*---------------------------------------------------------------------------*/
static int //IP Address 
construct_client_id(void)
{
  int len = snprintf(client_id, BUFFER_SIZE, "d:%s:%s:%02x%02x%02x%02x%02x%02x",
                     conf->org_id, conf->type_id,
                     linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
                     linkaddr_node_addr.u8[2], linkaddr_node_addr.u8[5],
                     linkaddr_node_addr.u8[6], linkaddr_node_addr.u8[7]);


  if(len < 0 || len >= BUFFER_SIZE) {
    printf("Client ID: %d, Buffer %d\n", len, BUFFER_SIZE);
    return 0;
  }

  return 1;
}
/*---------------------------------------------------------------------------*/
static void //Configuration
update_config(void)
{
  if(construct_client_id() == 0) {
 
    state = MQTT_CLIENT_STATE_CONFIG_ERROR;
    return;
  }

  if(construct_sub_topic() == 0) {
   
    state = MQTT_CLIENT_STATE_CONFIG_ERROR;
    return;
  }

  if(construct_pub_topic() == 0) {

    state = MQTT_CLIENT_STATE_CONFIG_ERROR;
    return;
  }

  seq_nr_value = 0;

  state = MQTT_CLIENT_STATE_INIT;

  etimer_set(&publish_periodic_timer, 0);

  return;
}
/*---------------------------------------------------------------------------*/
static int //Initial data of cloud 
init_config()
{
  /* Configuration data of the cloud */
  memset(conf, 0, sizeof(mqtt_client_config_t));

  memcpy(conf->org_id, CC26XX_WEB_DEMO_DEFAULT_ORG_ID, 11);
  memcpy(conf->type_id, CC26XX_WEB_DEMO_DEFAULT_TYPE_ID, 7);
  memcpy(conf->auth_token, CC26XX_WEB_DEMO_DEFAULT_AUTH_TOKEN, 9);
  memcpy(conf->event_type_id, CC26XX_WEB_DEMO_DEFAULT_EVENT_TYPE_ID, 7);
  memcpy(conf->broker_ip, broker_ip, strlen(broker_ip));
  memcpy(conf->cmd_type, CC26XX_WEB_DEMO_DEFAULT_SUBSCRIBE_CMD_TYPE, 1);

  conf->broker_port = CC26XX_WEB_DEMO_DEFAULT_BROKER_PORT;
  conf->pub_interval = CC26XX_WEB_DEMO_DEFAULT_PUBLISH_INTERVAL;

  return 1;
}
/*---------------------------------------------------------------------------*/
static void
register_http_post_handlers(void)
{
  httpd_simple_register_post_handler(&org_id_handler);
  httpd_simple_register_post_handler(&type_id_handler);
  httpd_simple_register_post_handler(&event_type_id_handler);
  httpd_simple_register_post_handler(&cmd_type_handler);
  httpd_simple_register_post_handler(&auth_token_handler);
  httpd_simple_register_post_handler(&interval_handler);
  httpd_simple_register_post_handler(&port_handler);
  httpd_simple_register_post_handler(&ip_addr_handler);
  httpd_simple_register_post_handler(&reconnect_handler);
}
/*---------------------------------------------------------------------------*/
static void 
subscribe(void)
{
  mqtt_status_t status;

  status = mqtt_subscribe(&conn, NULL, sub_topic, MQTT_QOS_LEVEL_0);

  DBG("APP - Subscribing!\n");
  if(status == MQTT_STATUS_OUT_QUEUE_FULL) {
    DBG("APP - Tried to subscribe but command queue was full!\n");
  }
}
/*---------------------------------------------------------------------------*/
extern void
publish(void)    //Pubblic Temperature, Volt Battery and I/O 
{
  /* PuBblic topic MQTT */
  int len;
  int remaining = APP_BUFFER_SIZE;
  char def_rt_str[64];
  char state_cen[64];

  seq_nr_value++;

  buf_ptr = app_buffer;
/******************************************************************************************/
  len = snprintf(buf_ptr, remaining,
                 "{"
                 "\"d\":{"
                 "\"myName\":\"%s\","
                 "\"Seq #\":%d,"
                 "\"Uptime\":%lu",
                 BOARD_STRING, seq_nr_value, clock_seconds());

  if(len < 0 || len >= remaining) {
    printf("Buffer too short. Have %d, need %d + \\0\n", remaining, len);
    return;
  }

  remaining -= len;
  buf_ptr += len;
/*****************************************************************************************/
  memset(def_rt_str, 0, sizeof(def_rt_str));
  cc26xx_web_demo_ipaddr_sprintf(def_rt_str, sizeof(def_rt_str),
                                 uip_ds6_defrt_choose());

  len = snprintf(buf_ptr, remaining, ",\"Def Route\":\"%s\",\"RSSI (dBm)\":%d",
                 def_rt_str, def_rt_rssi);

  if(len < 0 || len >= remaining) {
    printf("Buffer too short. Have %d, need %d + \\0\n", remaining, len);
    return;
  }
  remaining -= len;
  buf_ptr += len;

  memcpy(&def_route, uip_ds6_defrt_choose(), sizeof(uip_ip6addr_t));
/******************************************************************************************/
  for(reading = cc26xx_web_demo_sensor_first();
      reading != NULL; reading = reading->next) {
    if(reading->publish && reading->raw != CC26XX_SENSOR_READING_ERROR) {
      len = snprintf(buf_ptr, remaining,
                     ",\"%s\":%s", reading->descr,
                     reading->converted);

      if(len < 0 || len >= remaining) {
        printf("Buffer too short. Have %d, need %d + \\0\n", remaining, len);
        return;
      }
      remaining -= len;
      buf_ptr += len;
    }
  }
/******************************************************************************************/
  len = snprintf(buf_ptr, remaining, "}}");

  if(len < 0 || len >= remaining) {
    printf("Buffer too short. Have %d, need %d + \\0\n", remaining, len);
    return;
  }

  mqtt_publish(&conn, NULL, pub_topic, (uint8_t *)app_buffer,
               strlen(app_buffer), MQTT_QOS_LEVEL_0, MQTT_RETAIN_OFF);

  DBG("APP - Publish!\n");
}
/*---------------------------------------------------------------------------*/
static void //connecting to broker
connect_to_broker(void)
{
  /* Connect to server MQTT */
  mqtt_status_t conn_attempt_result = mqtt_connect(&conn, conf->broker_ip,
                                                   conf->broker_port,
                                                   conf->pub_interval * 3);

  if(conn_attempt_result == MQTT_STATUS_OK) {
    state = MQTT_CLIENT_STATE_CONNECTING;
  } else {
    state = MQTT_CLIENT_STATE_CONFIG_ERROR;
  }
}
/*---------------------------------------------------------------------------*/
static void //state device
state_machine(void)
{
  switch(state) {
  case MQTT_CLIENT_STATE_INIT:
    mqtt_register(&conn, &mqtt_client_process, client_id, mqtt_event,
                  MQTT_CLIENT_MAX_SEGMENT_SIZE);

    if(strncasecmp(conf->org_id, QUICKSTART, strlen(conf->org_id)) != 0) {
      if(strlen(conf->auth_token) == 0) {
        printf("User name set, but empty auth token\n");
        state = MQTT_CLIENT_STATE_ERROR;
        break;
      } else {
        mqtt_set_username_password(&conn, "use-token-auth",  //use-token-auth
                                   conf->auth_token);
      }
    }

    conn.auto_reconnect = 0;
    connect_attempt = 1;

    memset(&def_route, 0, sizeof(def_route));

    state = MQTT_CLIENT_STATE_REGISTERED;
    DBG("Init\n");
    /* Continue */
  case MQTT_CLIENT_STATE_REGISTERED:
    if(uip_ds6_get_global(ADDR_PREFERRED) != NULL) {
      /* Log-in with pubblic address. Connect */
      DBG("Registered. Connect attempt %u\n", connect_attempt);
      connect_to_broker();
    }
    etimer_set(&publish_periodic_timer, CC26XX_WEB_DEMO_NET_CONNECT_PERIODIC);
    return;
    break;
  case MQTT_CLIENT_STATE_CONNECTING:
    leds_on(CC26XX_WEB_DEMO_STATUS_LED);
    ctimer_set(&ct, CONNECTING_LED_DURATION, publish_led_off, NULL);
    /* Not connected yet. Wait */
    DBG("Connecting (%u)\n", connect_attempt);
    break;
  case MQTT_CLIENT_STATE_CONNECTED:
    /* Don't subscribe unless we are a registered device */
    if(strncasecmp(conf->org_id, QUICKSTART, strlen(conf->org_id)) == 0) {
      DBG("Using 'quickstart': Skipping subscribe\n");
      state = MQTT_CLIENT_STATE_PUBLISHING;
    }
    /* Continue */
  case MQTT_CLIENT_STATE_PUBLISHING:
    /* If the timer expired, the connection is stable. */
    if(timer_expired(&connection_life)) {
      connect_attempt = 0;
    }

  if(mqtt_ready(&conn) && conn.out_buffer_sent) {  
      /* Connected. Publish */
      if(state == MQTT_CLIENT_STATE_CONNECTED) {
     //   subscribe();
        state = MQTT_CLIENT_STATE_PUBLISHING;
      } else {
        leds_on(CC26XX_WEB_DEMO_STATUS_LED);
        ctimer_set(&ct, PUBLISH_LED_ON_DURATION, publish_led_off, NULL);
        publish();
        leds_on(LEDS_GREEN);
     }
      etimer_set(&publish_periodic_timer, conf->pub_interval);


      DBG("Publishing\n");

      return;
    } else {
      DBG("Publishing... (MQTT state=%d, q=%u)\n", conn.state,
          conn.out_queue_full);
    }
    break;
  case MQTT_CLIENT_STATE_DISCONNECTED:
    DBG("Disconnected\n");
    if(connect_attempt < RECONNECT_ATTEMPTS ||
       RECONNECT_ATTEMPTS == RETRY_FOREVER) {
      clock_time_t interval;
      mqtt_disconnect(&conn);
      connect_attempt++;
      

      interval = connect_attempt < 3 ? RECONNECT_INTERVAL << connect_attempt :
        RECONNECT_INTERVAL << 3;

      DBG("Disconnected. Attempt %u in %lu ticks\n", connect_attempt, interval);

      etimer_set(&publish_periodic_timer, interval);

      state = MQTT_CLIENT_STATE_REGISTERED;
      return;
    } else {
 
      state = MQTT_CLIENT_STATE_ERROR;
      DBG("Aborting connection after %u attempts\n", connect_attempt - 1);
    }
    break;
  case MQTT_CLIENT_STATE_NEWCONFIG:

    if(conn.state == MQTT_CONN_STATE_NOT_CONNECTED || conn.state == MQTT_CONN_STATE_ERROR) {
      update_config();
      DBG("New config\n");

      return;
    }
    break;
  case MQTT_CLIENT_STATE_CONFIG_ERROR:
 
    printf("Bad configuration.\n");
    return;
  case MQTT_CLIENT_STATE_ERROR:
  default:
    leds_on(CC26XX_WEB_DEMO_STATUS_LED);

    printf("Default case: State=0x%02x\n", state);
    return;
  }

  etimer_set(&publish_periodic_timer, STATE_MACHINE_PERIODIC);
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(mqtt_client_process, ev, data)
{

  PROCESS_BEGIN();

  printf("CC26XX MQTT Client Process\n");

  conf = &cc26xx_web_demo_config.mqtt_config;
  if(init_config() != 1) {
    PROCESS_EXIT();
  }

  register_http_post_handlers();

  update_config();

  while(1) {

    PROCESS_YIELD();

    if(ev == sensors_event && data == CC26XX_WEB_DEMO_MQTT_PUBLISH_TRIGGER) {
      if(state == MQTT_CLIENT_STATE_ERROR) {
        connect_attempt = 1;
        state = MQTT_CLIENT_STATE_REGISTERED;
      }
    }

    if(ev == httpd_simple_event_new_config) {

      etimer_set(&publish_periodic_timer, NEW_CONFIG_WAIT_INTERVAL);
    }

    if((ev == PROCESS_EVENT_TIMER && data == &publish_periodic_timer) ||
       ev == PROCESS_EVENT_POLL ||
       ev == cc26xx_web_demo_publish_event ||
       (ev == sensors_event && data == CC26XX_WEB_DEMO_MQTT_PUBLISH_TRIGGER)) {
      state_machine();
    }

    if(ev == cc26xx_web_demo_load_config_defaults) {
      init_config();
      etimer_set(&publish_periodic_timer, NEW_CONFIG_WAIT_INTERVAL);
    }
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
/**
 * @}
 */
