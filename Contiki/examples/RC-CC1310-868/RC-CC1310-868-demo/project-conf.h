/*
 * Copyright (c) RadioControlli 10/2018
 * ver 1.0
 *
 * Stack configuration
 *
 */
/*---------------------------------------------------------------------------*/
#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_
/*---------------------------------------------------------------------------*/
#define IEEE802154_CONF_PANID            0xABCD
#define RF_CORE_CONF_CHANNEL                 25
#define RF_BLE_CONF_ENABLED                   1

#undef NETSTACK_CONF_RDC 
#define NETSTACK_CONF_RDC    nullrdc_driver
/*---------------------------------------------------------------------------*/
/* Enable/Disable process */
#define CC26XX_WEB_DEMO_CONF_MQTT_CLIENT      1
#define CC26XX_WEB_DEMO_CONF_6LBR_CLIENT      0
#define CC26XX_WEB_DEMO_CONF_COAP_SERVER      0
#define CC26XX_WEB_DEMO_CONF_ADC_DEMO         0
/*---------------------------------------------------------------------------*/
/* Enable ROM */
#define ROM_BOOTLOADER_ENABLE                 1
/*---------------------------------------------------------------------------*/
#define UIP_CONF_BUFFER_SIZE                900
#define NBR_TABLE_CONF_MAX_NEIGHBORS          5
#define UIP_CONF_MAX_ROUTES                   5
#define UIP_CONF_TCP_MSS                    128
/*---------------------------------------------------------------------------*/
#endif /* PROJECT_CONF_H_ */
/*---------------------------------------------------------------------------*/
