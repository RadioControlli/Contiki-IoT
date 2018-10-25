/*
 * Copyright (c) 03/2018, RadioControlli srl -
 * ver 1.0
 * All rights reserved.
 *
 * \file
 *      CoAP resource handler for the RCCC1310 LEDs. Slightly modified copy of
 *      the one found in Contiki's original CoAP example.
 */
/*---------------------------------------------------------------------------*/
#include "contiki.h"
#include "rest-engine.h"
#include "dev/leds.h"

#include <string.h>
/*---------------------------------------------------------------------------*/
static void
res_post_put_handler(void *request, void *response, uint8_t *buffer,
                     uint16_t preferred_size, int32_t *offset)
{
  size_t len = 0;
  const char *color = NULL;
  const char *mode = NULL;
  uint8_t led = 0;
  int success = 1;

  if((len = REST.get_query_variable(request, "color", &color))) {
    if(strncmp(color, "r", len) == 0) {
      led = LEDS_RED;
    } else if(strncmp(color, "g", len) == 0) {
      led = LEDS_GREEN;
#if BOARD_SMARTRF06EB
    } else if(strncmp(color, "y", len) == 0) {
      led = LEDS_YELLOW;
    } else if(strncmp(color, "o", len) == 0) {
      led = LEDS_ORANGE;
#endif
    } else {
      success = 0;
    }
  } else {
    success = 0;
  }

  if(success && (len = REST.get_post_variable(request, "mode", &mode))) {
    if(strncmp(mode, "on", len) == 0) {
      leds_on(led);
    } else if(strncmp(mode, "off", len) == 0) {
      leds_off(led);
    } else {
      success = 0;
    }
  } else {
    success = 0;
  }

  if(!success) {
    REST.set_response_status(response, REST.status.BAD_REQUEST);
  }
}
/*---------------------------------------------------------------------------*/
/*
 * A simple actuator example, depending on the color query parameter and post
 * variable mode, corresponding led is activated or deactivated
 */
#if BOARD_SENSORTAG || BOARD_LAUNCHPAD
#define RESOURCE_PARAMS "r|g"
#elif BOARD_SMARTRF06EB
#define RESOURCE_PARAMS "r|g|y|o"
#endif

RESOURCE(res_leds,
         "title=\"LEDs: ?color=" RESOURCE_PARAMS ", POST/PUT mode=on|off\";rt=\"Control\"",
         NULL,
         res_post_put_handler,
         res_post_put_handler,
         NULL);
/*---------------------------------------------------------------------------*/
/** @} */
