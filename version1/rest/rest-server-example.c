#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "contiki.h"
#include "contiki-net.h"
#include "rest.h"

#if defined (PLATFORM_HAS_LIGHT)
#include "dev/light-sensor.h"
#endif
#if defined (PLATFORM_HAS_BATT)
#include "dev/battery-sensor.h"
#endif
#if defined (PLATFORM_HAS_SHT11)
#include "dev/sht11-sensor.h"
#endif
#if defined (PLATFORM_HAS_LEDS)
#include "dev/leds.h"
#endif

#define DEBUG 1
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#define PRINT6ADDR(addr) PRINTF(" %02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x ", ((u8_t *)addr)[0], ((u8_t *)addr)[1], ((u8_t *)addr)[2], ((u8_t *)addr)[3], ((u8_t *)addr)[4], ((u8_t *)addr)[5], ((u8_t *)addr)[6], ((u8_t *)addr)[7], ((u8_t *)addr)[8], ((u8_t *)addr)[9], ((u8_t *)addr)[10], ((u8_t *)addr)[11], ((u8_t *)addr)[12], ((u8_t *)addr)[13], ((u8_t *)addr)[14], ((u8_t *)addr)[15])
#define PRINTLLADDR(lladdr) PRINTF(" %02x:%02x:%02x:%02x:%02x:%02x ",(lladdr)->addr[0], (lladdr)->addr[1], (lladdr)->addr[2], (lladdr)->addr[3],(lladdr)->addr[4], (lladdr)->addr[5])
#else
#define PRINTF(...)
#define PRINT6ADDR(addr)
#define PRINTLLADDR(addr)
#endif

char temp[100];

// LOS RECURSOS HTTP SE DEFINEN CON LA MACRO RESOURCE: NOMBRE DEL RECURSO, EL METODO HTTP QUE LO MANEJA Y SU URL
RESOURCE(helloworld, METHOD_GET, "helloworld");

// PARA CADA RECURSO DEFINIDO, LE CORRESPONDE UN METODO HANDLER QUE DEBERIA DEFINIRSE TAMBIEN.
// EL NOMBRE DEL HANDLER DEBERIA SER [nombre recurso]_handler
void helloworld_handler(REQUEST* request, RESPONSE* response)
{
  	// ESCRIBE LA SALIDA EN LA CADENA DE CHARS "temp"
  	sprintf(temp,"Hello World!\n");
  	// TIPO DE RESPUESTA HTTP
  	rest_set_header_content_type(response, TEXT_PLAIN);
  	// LA RESPUESTA EN SÍ, LA CARGA ÚTIL
  	rest_set_response_payload(response, (uint8_t*)temp, strlen(temp));
}

//////// OTRO RECURSO ////////
RESOURCE(discover, METHOD_GET, ".well-known/core");
void discover_handler(REQUEST* request, RESPONSE* response)
{
  	char temp[100];
  	int index = 0;
  	index += sprintf(temp + index, "%s,", "</helloworld>;n=\"HelloWorld\"");
#if defined (PLATFORM_HAS_LEDS)
  	index += sprintf(temp + index, "%s,", "</led>;n=\"LedControl\"");
#endif
#if defined (PLATFORM_HAS_LIGHT)
  	index += sprintf(temp + index, "%s", "</light>;n=\"Light\"");
#endif

  	rest_set_response_payload(response, (uint8_t*)temp, strlen(temp));
  	rest_set_header_content_type(response, APPLICATION_LINK_FORMAT);
}

#if defined (PLATFORM_HAS_LIGHT)
uint16_t light_photosynthetic;
uint16_t light_solar;

void read_light_sensor(uint16_t* light_1, uint16_t* light_2)
{
  	*light_1 = light_sensor.value(LIGHT_SENSOR_PHOTOSYNTHETIC);
  	*light_2 = light_sensor.value(LIGHT_SENSOR_TOTAL_SOLAR);
}

// OTRO RECURSO: UN EJEMPLO SIMPLE DE UN GETTER. DEVUELVE LA LECTURA DEL SENSOR DE LUZ CON UN SIMPLE "etag"
RESOURCE(light, METHOD_GET, "light");
void light_handler(REQUEST* request, RESPONSE* response)
{
  	read_light_sensor(&light_photosynthetic, &light_solar);
  	sprintf(temp,"%u;%u", light_photosynthetic, light_solar);

  	char etag[4] = "ABCD";
  	rest_set_header_content_type(response, TEXT_PLAIN);
  	rest_set_header_etag(response, etag, sizeof(etag));
  	rest_set_response_payload(response, temp, strlen(temp));
}
#endif

#if defined (PLATFORM_HAS_LEDS)
// OTRO RECURSO: UN EJEMPLO DE ACTUADOR SIMPLE, dependiendo del parámetro de consulta de color 
// y del modo de variable de post, se activa o se desactiva el led correspondiente
RESOURCE(led, METHOD_POST | METHOD_PUT , "led");
void led_handler(REQUEST* request, RESPONSE* response)
{
  	char color[10];
  	char mode[10];
  	uint8_t led = 0;
  	int success = 1;
  	// ACCEDE A LOS PARAMETROS QUE VIENEN EN LA URL
  	// DEVUELVE "TRUE" SI ENCUENTRA LA VARIABLE "color" y LA GUARDA EN EL BUFFER color
  	if (rest_get_query_variable(request, "color", color, 10)) {
    	PRINTF("color %s\n", color);
    	// AQUI MIRA SI COINCIDE CON ALGUN COLOR DE ESTOS
    	if (!strcmp(color,"red")) {
      		led = LEDS_RED;
    	}else if(!strcmp(color,"green")) {
      		led = LEDS_GREEN;
    	}else if ( !strcmp(color,"blue") ) {
      		led = LEDS_BLUE;
    	}else {
      		success = 0;
    	}
  	}else{
    	success = 0;
  	}
  	// ACCEDE A LAS VARIABLES QUE VIENEN EN EL CUERPO DE LA PETICION
  	// DEVUELVE TRUE SI LA VARIABLE EXISTE, Y LA PONE EN EL BUFFER PROPORCIONADO
  	if (success && rest_get_post_variable(request, "mode", mode, 10)) {
    	PRINTF("mode %s\n", mode);
    	if (!strcmp(mode, "on")) {
      		leds_on(led);
    	} else if (!strcmp(mode, "off")) {
      		leds_off(led);
    	}else {
      		success = 0;
    	}
  	}else{
    	success = 0;
  	}

  	if (!success) {
    	rest_set_response_status(response, BAD_REQUEST_400);
  	}
}

// UN SIMPLE EJEMPLO DE ACTUADOR. CAMBIA EL LED ROJO
RESOURCE(toggle, METHOD_GET | METHOD_PUT | METHOD_POST, "toggle");
void toggle_handler(REQUEST* request, RESPONSE* response)
{
	leds_toggle(LEDS_RED);
}

#endif // defined (CONTIKI_HAS_LEDS)


PROCESS(rest_server_example, "Rest Server Example");	// PCB
AUTOSTART_PROCESSES(&rest_server_example);				// AUTOARRANCA EL PROCESO

PROCESS_THREAD(rest_server_example, ev, data)			// PROCESS THREAD
{
  	PROCESS_BEGIN();										// INICIA EL PROTOTHREAD

#ifdef WITH_COAP
  	PRINTF("COAP Server\n");
#else
  	PRINTF("HTTP Server\n");
#endif
  	// LLAMA A ESTA FUNCION DE rest.c QUE INVOCA UN
  	// SERVIDOR COAP O HTTP SEGUN SE TERCIE
  	rest_init();

#if defined (PLATFORM_HAS_LIGHT)
  	SENSORS_ACTIVATE(light_sensor);
  	// ACTIVAMOS LOS RECURSOS, SI NO, NO PODRAN SER ACCEDIDOS
  	rest_activate_resource(&resource_light);
#endif
#if defined (PLATFORM_HAS_LEDS)
  	rest_activate_resource(&resource_led);
  	rest_activate_resource(&resource_toggle);
#endif
  	rest_activate_resource(&resource_helloworld);
  	rest_activate_resource(&resource_discover);

  	PROCESS_END();										// ACABA EL PROTOTHREAD
}