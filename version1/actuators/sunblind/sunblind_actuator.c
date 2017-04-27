#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "contiki.h"
#include "contiki-net.h"

#include "erbium.h"

#include "dev/leds.h"

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

RESOURCE(leds, METHOD_POST | METHOD_PUT , "actuators/sunblind",
		 "title=\"level: status=up|middle-up|medium|middle-down|down \";rt=\"Control\"");

void leds_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
    size_t len = 0;
    const char *status = NULL;
    int success = 1;
    // accede a los parametros que vienen EN EL CUERPO DE LA PETICIÓN
    if ((len=REST.get_post_variable(request, "status", &status))){
        PRINTF("status %.*s\n", len, status);
        // Compara las dos cadenas, pero solo los LEN PRIMEROS caracteres
        if (strncmp(status, "up", len)==0) {
            leds_on(LEDS_RED);
            leds_off(LEDS_GREEN);
            leds_off(LEDS_BLUE);
        }else if (strncmp(status,"middle-up", len)==0) {
            leds_on(LEDS_RED);
            leds_on(LEDS_GREEN);
            leds_off(LEDS_BLUE);
        }else if (strncmp(status,"medium", len)==0) {
            leds_off(LEDS_RED);
            leds_on(LEDS_GREEN);
            leds_off(LEDS_BLUE);
        }else if (strncmp(status,"middle-down", len)==0) {
            leds_off(LEDS_RED);
            leds_on(LEDS_GREEN);
            leds_on(LEDS_BLUE);
        }else if (strncmp(status,"down", len)==0) {
            leds_off(LEDS_RED);
            leds_off(LEDS_GREEN);
            leds_on(LEDS_BLUE);
        }else {
            success = 0;
        }
    }else {
        success = 0;
    }

    if (!success) {
        REST.set_response_status(response, REST.status.BAD_REQUEST);
    }
}

PROCESS(sunblind_actuator, "Erbium Example Server");
AUTOSTART_PROCESSES(&sunblind_actuator);

PROCESS_THREAD(sunblind_actuator, ev, data)
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

    rest_activate_resource(&resource_leds);

    PROCESS_END();
}