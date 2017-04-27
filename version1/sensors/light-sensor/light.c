#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "contiki.h"
#include "contiki-net.h"

#if defined (PLATFORM_HAS_LIGHT)
	#include "dev/light-sensor.h"
#endif

#include "erbium.h"

// ESTAS SON LAS DIFERENTES VERSIONES DE COAP QUE IMPLEMENTA ERBIUM (AUNQUE HAY MÁS)
/* For CoAP-specific example: not required for normal RESTful Web service. */
#if WITH_COAP == 3
	#include "er-coap-03.h"
#elif WITH_COAP == 7
	#include "er-coap-07.h"
#elif WITH_COAP == 12
	#include "er-coap-12.h"
#elif WITH_COAP == 13
	#include "er-coap-13.h"
#else
	#warning "Erbium example without CoAP-specifc functionality"
#endif

#define DEBUG 0
#if DEBUG
	#define PRINTF(...) printf(__VA_ARGS__)
	#define PRINT6ADDR(addr) PRINTF("[%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x]", ((uint8_t *)addr)[0], ((uint8_t *)addr)[1], ((uint8_t *)addr)[2], ((uint8_t *)addr)[3], ((uint8_t *)addr)[4], ((uint8_t *)addr)[5], ((uint8_t *)addr)[6], ((uint8_t *)addr)[7], ((uint8_t *)addr)[8], ((uint8_t *)addr)[9], ((uint8_t *)addr)[10], ((uint8_t *)addr)[11], ((uint8_t *)addr)[12], ((uint8_t *)addr)[13], ((uint8_t *)addr)[14], ((uint8_t *)addr)[15])
	#define PRINTLLADDR(lladdr) PRINTF("[%02x:%02x:%02x:%02x:%02x:%02x]",(lladdr)->addr[0], (lladdr)->addr[1], (lladdr)->addr[2], (lladdr)->addr[3],(lladdr)->addr[4], (lladdr)->addr[5])
#else
	#define PRINTF(...)
	#define PRINT6ADDR(addr)
	#define PRINTLLADDR(addr)
#endif


#if defined (PLATFORM_HAS_LIGHT)
RESOURCE(light, METHOD_GET, "sensors/light", "title=\"Photosynthetic and solar light\";rt=\"LightSensor\"");
void light_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
  	
  	uint16_t fotosintetica = 40 * light_sensor.value(LIGHT_SENSOR_PHOTOSYNTHETIC) / 98;
  	uint16_t solar = 40 * light_sensor.value(LIGHT_SENSOR_TOTAL_SOLAR) / 98;
  	REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
    snprintf((char *)buffer, REST_MAX_CHUNK_SIZE, "Photosynthetic: %u lux\nSolar: %u lux", fotosintetica, solar);
    REST.set_response_payload(response, (uint8_t *)buffer, strlen((char *)buffer));
}
#endif 


PROCESS(light_sens, "Erbium Example Server");
AUTOSTART_PROCESSES(&light_sens);

PROCESS_THREAD(light_sens, ev, data)
{
  	PROCESS_BEGIN();

#ifdef RF_CHANNEL
  	PRINTF("RF channel: %u\n", RF_CHANNEL);
#endif
#ifdef IEEE802154_PANID
  	PRINTF("PAN ID: 0x%04X\n", IEEE802154_PANID);
#endif

  	PRINTF("uIP buffer: %u\n", UIP_BUFSIZE);
  	PRINTF("LL header: %u\n", UIP_LLH_LEN);
  	PRINTF("IP+UDP header: %u\n", UIP_IPUDPH_LEN);
  	PRINTF("REST max chunk: %u\n", REST_MAX_CHUNK_SIZE);

  	rest_init_engine();

	// ACTIVAMOS LOS RECURSOS, SI NO, NO PODRAN SER ACCEDIDOS
#if defined (PLATFORM_HAS_LIGHT)
  	SENSORS_ACTIVATE(light_sensor);
  	rest_activate_resource(&resource_light);
#endif

  	PROCESS_END();
}