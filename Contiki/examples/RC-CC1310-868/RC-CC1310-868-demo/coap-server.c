/*
 * Copyright (c) 03/2018, RadioControlli srl -
 * All rights reserved.
 * ver 1.0
 *
 * \file
 *     A RCCC1310-specific CoAP server
 */
/*---------------------------------------------------------------------------*/
#include "contiki.h"
#include "contiki-net.h"
#include "rest-engine.h"
#include "board-peripherals.h"
#include "rf-core/rf-ble.h"
#include "RC-CC1310-868-demo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/*---------------------------------------------------------------------------*/
/* Common resources */
extern resource_t res_leds;

extern resource_t res_batmon_temp;
extern resource_t res_batmon_volt;

extern resource_t res_device_sw;
extern resource_t res_device_hw;
extern resource_t res_device_uptime;
extern resource_t res_device_cfg_reset;

extern resource_t res_parent_rssi;
extern resource_t res_parent_ip;

#if RF_BLE_ENABLED
extern resource_t res_ble_advd;
#endif

extern resource_t res_toggle_red;
extern resource_t res_toggle_green;

/* Board-specific resources */
/*#if BOARD_SENSORTAG
extern resource_t res_bmp280_temp;
extern resource_t res_bmp280_press;
extern resource_t res_tmp007_amb;
extern resource_t res_tmp007_obj;
extern resource_t res_hdc1000_temp;
extern resource_t res_hdc1000_hum;
extern resource_t res_opt3001_light;
extern resource_t res_mpu_acc_x;
extern resource_t res_mpu_acc_y;
extern resource_t res_mpu_acc_z;
extern resource_t res_mpu_gyro_x;
extern resource_t res_mpu_gyro_y;
extern resource_t res_mpu_gyro_z;
#else */
extern resource_t res_toggle_orange;
extern resource_t res_toggle_yellow;
//#endif

#if CC26XX_WEB_DEMO_ADC_DEMO
extern resource_t res_adc_dio23;
#endif
/*---------------------------------------------------------------------------*/
const char *coap_server_not_found_msg = "Resource not found";
const char *coap_server_supported_msg = "Supported:"
                                        "text/plain,"
                                        "application/json,"
                                        "application/xml";
/*---------------------------------------------------------------------------*/
static void
start_board_resources(void)
{

  rest_activate_resource(&res_toggle_green, "lt/g");
  rest_activate_resource(&res_toggle_red, "lt/r");
  rest_activate_resource(&res_leds, "lt");

/*#if BOARD_SENSORTAG
  rest_activate_resource(&res_bmp280_temp, "sen/bar/temp");
  rest_activate_resource(&res_bmp280_press, "sen/bar/pres");
  rest_activate_resource(&res_tmp007_amb, "sen/tmp/amb");
  rest_activate_resource(&res_tmp007_obj, "sen/tmp/obj");
  rest_activate_resource(&res_hdc1000_temp, "sen/hdc/t");
  rest_activate_resource(&res_hdc1000_hum, "sen/hdc/h");
  rest_activate_resource(&res_opt3001_light, "sen/opt/light");
  rest_activate_resource(&res_mpu_acc_x, "sen/mpu/acc/x");
  rest_activate_resource(&res_mpu_acc_y, "sen/mpu/acc/y");
  rest_activate_resource(&res_mpu_acc_z, "sen/mpu/acc/z");
  rest_activate_resource(&res_mpu_gyro_x, "sen/mpu/gyro/x");
  rest_activate_resource(&res_mpu_gyro_y, "sen/mpu/gyro/y");
  rest_activate_resource(&res_mpu_gyro_z, "sen/mpu/gyro/z");
#elif BOARD_SMARTRF06EB
  rest_activate_resource(&res_toggle_yellow, "lt/y");
  rest_activate_resource(&res_toggle_orange, "lt/o");
#endif */
}
/*---------------------------------------------------------------------------*/
PROCESS(coap_server_process, "CC26XX CoAP Server");
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(coap_server_process, ev, data)
{
  PROCESS_BEGIN();

  printf("CC26XX CoAP Server\n");

  /* Initialize the REST engine. */
  rest_init_engine();

  rest_activate_resource(&res_batmon_temp, "sen/batmon/temp");
  rest_activate_resource(&res_batmon_volt, "sen/batmon/voltage");

#if CC26XX_WEB_DEMO_ADC_DEMO
  rest_activate_resource(&res_adc_dio23, "sen/adc/dio23");
#endif

  rest_activate_resource(&res_device_hw, "dev/mdl/hw");
  rest_activate_resource(&res_device_sw, "dev/mdl/sw");
  rest_activate_resource(&res_device_uptime, "dev/uptime");
  rest_activate_resource(&res_device_cfg_reset, "dev/cfg_reset");

  rest_activate_resource(&res_parent_rssi, "net/parent/RSSI");
  rest_activate_resource(&res_parent_ip, "net/parent/IPv6");

#if RF_BLE_ENABLED
  rest_activate_resource(&res_ble_advd, "dev/ble_advd");
#endif

  start_board_resources();

  /* Define application-specific events here. */
  while(1) {
    PROCESS_WAIT_EVENT();
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
/**
 * @}
 */
