#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "contiki.h"
#include "contiki-net.h"

// Define qué recursos incluir para cumplir con las restricciones de memoria.
// OJO! SÓLO puedes tener activados 4 RECURSOS, si no "peta" la memoria
#define REST_RES_HELLO 0
#define REST_RES_LIGHT 0
#define REST_RES_CHUNKS 0
#define REST_RES_PUSHING 0
#define REST_RES_SUB 0
#define REST_RES_EVENT 0
#define REST_RES_LEDS 1
#define REST_RES_SEPARATE 0

/*
#define REST_RES_BATTERY 0
#define REST_RES_RADIO 0
#define REST_RES_MIRROR 0 // causes largest code size */

#include "erbium.h"

#if defined (PLATFORM_HAS_BUTTON)
	#include "dev/button-sensor.h"
#endif
#if defined (PLATFORM_HAS_LEDS)
	#include "dev/leds.h"
#endif
#if defined (PLATFORM_HAS_LIGHT)
	#include "dev/light-sensor.h"
#endif
#if defined (PLATFORM_HAS_BATTERY)
	#include "dev/battery-sensor.h"
#endif
#if defined (PLATFORM_HAS_SHT11)
	#include "dev/sht11-sensor.h"
#endif
#if defined (PLATFORM_HAS_RADIO)
	#include "dev/radio-sensor.h"
#endif
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

#if REST_RES_HELLO

// 		  /NOMBRE/  /METODO HTTP/ /URL/
RESOURCE(helloworld, METHOD_GET, "hello", "title=\"Hello world: ?len=0..\";rt=\"Text\"");
// PARA CADA RECURSO DEFINIDO, LE CORRESPONDE UN METODO HANDLER 
// EL NOMBRE DEL HANDLER DEBERIA SER [nombre recurso]_handler
void helloworld_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
  	const char *len = NULL;
  	char const * const message = "Hello World! ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxy";
  	int length = 12; //12 chars=> |<-------->|

	// ACCEDE A LOS PARAMETROS QUE VIENEN EN LA URL
  	// DEVUELVE "TRUE" SI ENCUENTRA LA VARIABLE "len", y LA GUARDA EN EL BUFFER len
  	if (REST.get_query_variable(request, "len", &len)) {
    	// atoi (cadena de chars --> entero)
    	length = atoi(len);
    	if(length<0) length = 0;
    	if (length>REST_MAX_CHUNK_SIZE) length = REST_MAX_CHUNK_SIZE;
    	memcpy(buffer, message, length);
  	}else {
    	memcpy(buffer, message, length);
  	}
  	// TIPO DE RESPUESTA HTTP
  	REST.set_header_content_type(response, REST.type.TEXT_PLAIN); 
  	REST.set_header_etag(response, (uint8_t *) &length, 1);
  	// LA RESPUESTA EN SÍ, LA CARGA ÚTIL
	REST.set_response_payload(response, buffer, length);
}
#endif

#if REST_RES_LIGHT && defined (PLATFORM_HAS_LIGHT)
RESOURCE(light, METHOD_GET, "sensors/light", "title=\"Photosynthetic and solar light (supports JSON)\";rt=\"LightSensor\"");
void light_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
  	uint16_t light_photosynthetic = light_sensor.value(LIGHT_SENSOR_PHOTOSYNTHETIC);
  	uint16_t light_solar = light_sensor.value(LIGHT_SENSOR_TOTAL_SOLAR);

  	const uint16_t *accept = NULL;
  	//	cogemos la CABECERA ACCEPT
  	int existe = REST.get_header_accept(request, &accept);
  	//	AQUI CHEQUEAMOS EL FORMATO QUE ACEPTA COMO RESPUESTA
  	//  text-plain
  	if ((existe==0) || (existe && accept[0]==REST.type.TEXT_PLAIN)){
    	REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
    	snprintf((char *)buffer, REST_MAX_CHUNK_SIZE, "%u;%u", light_photosynthetic, light_solar);
    	REST.set_response_payload(response, (uint8_t *)buffer, strlen((char *)buffer));
  	}
  	//	XML
  	else if (existe && (accept[0]==REST.type.APPLICATION_XML)){
	    REST.set_header_content_type(response, REST.type.APPLICATION_XML);
    	snprintf((char *)buffer, REST_MAX_CHUNK_SIZE, "<light photosynthetic=\"%u\" solar=\"%u\"/>", light_photosynthetic, light_solar);

    	REST.set_response_payload(response, buffer, strlen((char *)buffer));
  	}
  	//	JSON
  	else if (existe && (accept[0]==REST.type.APPLICATION_JSON)){
	    REST.set_header_content_type(response, REST.type.APPLICATION_JSON);
    	snprintf((char *)buffer, REST_MAX_CHUNK_SIZE, "{'light':{'photosynthetic':%u,'solar':%u}}", light_photosynthetic, light_solar);

    	REST.set_response_payload(response, buffer, strlen((char *)buffer));
  	}
  	//	MAL HECHA la petición
  	else{
    	REST.set_response_status(response, REST.status.NOT_ACCEPTABLE);
    	const char *msg = "Supporting content-types text/plain, application/xml, and application/json";
    	REST.set_response_payload(response, msg, strlen(msg));
  	}
}
#endif 

#if REST_RES_CHUNKS
 /* Para los datos mayores que REST_MAX_CHUNK_SIZE (por ejemplo, almacenados en flash),
 los recursos deben tener en cuenta la limitación del búfer y DIVIDIR LAS RESPUESTAS POR SÍ MISMOS.
 Para transferir el recurso completo a través de un flujo TCP o transferencia de bloques de CoAP,
 el desplazamiento de bytes donde continuar se proporciona al manejador como puntero int32_t.
 Estos recursos en forma de trozos deben establecer el valor de compensación en su nueva posición
 o -1 se alcanza el final. (El desplazamiento para la transferencia de bloques de CoAP puede ir 
 hasta 2'147'481'600 = ~ 2047 M para el tamaño de bloque 2048 (reducido a 1024 en observar-03). */
RESOURCE(chunks, METHOD_GET, "test/chunks", "title=\"Blockwise demo\";rt=\"Data\"");
#define CHUNKS_TOTAL    67											
void chunks_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
	int32_t strpos = 0;																	// Va de 64 en 64 (0,64,128...)
																						// en cada petición
  	// Si el desplazamiento ES MAYOR que los BYTES TOTALES:
  	if (*offset>=CHUNKS_TOTAL){
	    REST.set_response_status(response, REST.status.BAD_OPTION);
    	// Un mensaje de error de bloque no debe exceder el tamaño mínimo del bloque (16).
    	const char *error_msg = "BlockOutOfScope";
    	REST.set_response_payload(response, error_msg, strlen(error_msg));
    	return;
  	}

  	// Generar datos hasta llegar a CHUNKS_TOTAL (sumando TODAS las peticiones)
  	while (strpos<preferred_size){
  				  //			/output/		 /nº de bytes a escribir/ /formato/ /variable/
	    strpos += snprintf((char *)buffer+strpos, preferred_size-strpos+1, "|%ld|", *offset);
	    										// los bytes a escribir se van decrementando 
												// porque va aumentando strpos. Por muy grande
												// que sean los bytes a escribir, solo escribirá
	    										// 4 ó 5 bytes, ya que es lo máximo que hay (|64| o |128|)
  	}
  	// por si se pasa del tamaño
  	if (strpos > preferred_size){
	    strpos = preferred_size;
  	}
  	/* Truncate if above CHUNKS_TOTAL bytes. */
  	if (*offset+(int32_t)strpos > CHUNKS_TOTAL){
	    strpos = CHUNKS_TOTAL - *offset;
  	}

  	REST.set_response_payload(response, buffer, strpos);
	/* IMPORTANT for chunk-wise resources: Signal chunk awareness to REST engine. */
  	*offset += strpos;
  	// Señal del final de la representacion del recurso
  	if (*offset>=CHUNKS_TOTAL){
	    *offset = -1;
  	}
}
#endif

#if REST_RES_SUB
// Ejemplo de UN RECURSO que maneja SUB-RECURSOS
RESOURCE(sub, METHOD_GET | HAS_SUB_RESOURCES, "test/path", "title=\"Sub-resource demo\"");
void sub_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
			// DEVUELVE UN TEXTO PLANO
	REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
  	const char *uri_path = NULL;
  	//			   /longitud de la url DE LA PETICIÓN, además la guarda en uri_path/
  	int len = REST.get_url(request, &uri_path);
  	// 			   /longitud de la url DEL RECURSO (test/path)/
  	int base_len = strlen(resource_sub.url);
  	// si son iguales --> NO HAY SUB-RECURSOS
  	if (len==base_len){																	//   test/path	
		snprintf((char *)buffer, REST_MAX_CHUNK_SIZE, "Request any sub-resource of /%s", resource_sub.url);
  	}
  	else{
	    snprintf((char *)buffer, REST_MAX_CHUNK_SIZE, "%.*s", len-base_len, uri_path+base_len);
  	}
  	REST.set_response_payload(response, buffer, strlen((char *)buffer));
}
#endif

#if REST_RES_PUSHING
// Tiene un argumento EXTRA, que define el intervalo de llamada a la funcion [name]_periodic_handler().
PERIODIC_RESOURCE(pushing, METHOD_GET, "test/push", "title=\"Periodic demo\";obs", 5*CLOCK_SECOND);
                                                                                 // OJO CON EL TIEMPO!!
                                                                        // LA EJECUCIÓN NO VA A LA VELOCIDAD REAL!!
void pushing_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
    REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
    const char *msg = "It's periodic!";
    REST.set_response_payload(response, msg, strlen(msg));
}

void pushing_periodic_handler(resource_t *r){
    static uint16_t contador = 0;
    static char content[11];

    ++contador;

    PRINTF("TICK %u for /%s\n", contador, r->url);

    // Construír notificación
    coap_packet_t notification[1]; // De esta manera el paquete puede ser tratado como un puntero
    //                             /tipo respuesta/   /codigo/
    coap_init_message(notification, COAP_TYPE_NON, REST.status.OK, 0);
    //                             /payload/        /longitud del payload/
    coap_set_payload(notification, content, snprintf(content, sizeof(content), "TICK %u", contador));
    // Notificar a los observadores registrados con el tipo de mensaje dado, observar la opción y la carga útil.
    REST.notify_subscribers(r, contador, notification);
}
#endif

#if REST_RES_EVENT && defined (PLATFORM_HAS_BUTTON)
//  Example for an event resource
EVENT_RESOURCE(boton, METHOD_GET, "sensors/button", "title=\"Event demo\";obs");

void boton_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
    REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
    const char *msg = "It's eventful!";
    REST.set_response_payload(response, (uint8_t *)msg, strlen(msg));
}
// Un evento definido por el usuario activa este controlador, que puede ser una pulsación
// de botón o un PUT a otro recurso que provocó una actualización de estado.
void boton_event_handler(resource_t *r){
    static uint16_t contador_evento = 0;
    static char content[12];

    ++contador_evento;

    PRINTF("TICK %u for /%s\n", contador_evento, r->url);

    // Construír notificación    
    coap_packet_t notification[1]; // De esta manera el paquete puede ser tratado como un puntero
    //                             /tipo respuesta/   /codigo/
    coap_init_message(notification, COAP_TYPE_CON, REST.status.OK, 0 );
    //                             /payload/        /longitud del payload/
    coap_set_payload(notification, content, snprintf(content, sizeof(content), "EVENT %u", contador_evento));
    REST.notify_subscribers(r, contador_evento, notification);
}
#endif

#if defined (PLATFORM_HAS_LEDS) && REST_RES_LEDS
/*A simple actuator example, depending on the color query parameter and post variable mode,
corresponding led is activated or deactivated*/
RESOURCE(leds, METHOD_POST | METHOD_PUT , "actuators/leds", "title=\"LEDs: ?color=r|g|b, POST/PUT mode=on|off\";rt=\"Control\"");

void leds_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
    size_t len = 0;
    const char *color = NULL;
    const char *mode = NULL;
    uint8_t led = 0;
    int success = 1;
    // accede a los parametros que vienen EN LA URL
    // DEVUELVE "TRUE" SI ENCUENTRA LA VARIABLE "color", y LA GUARDA EN EL BUFFER color
    if ((len=REST.get_query_variable(request, "color", &color))){
        PRINTF("color %.*s\n", len, color);
        // Compara las dos cadenas, pero solo los LEN PRIMEROS caracteres
        if (strncmp(color, "r", len)==0) {
            led = LEDS_RED;
        }else if (strncmp(color,"g", len)==0) {
            led = LEDS_GREEN;
        }else if (strncmp(color,"b", len)==0) {
            led = LEDS_BLUE;
        }else {
            success = 0;
        }
    }else {
        success = 0;
    }
    // accede a los parametros que vienen EN EL CUERPO DE LA PETICION
    // DEVUELVE "TRUE" SI LA VARIABLE "mode" EXISTE, Y LA PONE EN EL BUFFER PROPORCIONADO
    if (success && (len=REST.get_post_variable(request, "mode", &mode))){
        PRINTF("mode %s\n", mode);
        if (strncmp(mode, "on", len)==0) {
            leds_on(led);
        }else if (strncmp(mode, "off", len)==0) {
            leds_off(led);
        }else {
            success = 0;
        }
    }else{
        success = 0;
    }

    if (!success) {
        REST.set_response_status(response, REST.status.BAD_REQUEST);
    }
}
#endif

#if REST_RES_SEPARATE && defined (PLATFORM_HAS_BUTTON) && WITH_COAP > 3
/* Required to manually (=not by the engine) handle the response transaction. */
#if WITH_COAP == 7
#include "er-coap-07-separate.h"
#include "er-coap-07-transactions.h"
#elif WITH_COAP == 12
#include "er-coap-12-separate.h"
#include "er-coap-12-transactions.h"
#elif WITH_COAP == 13
#include "er-coap-13-separate.h"
#include "er-coap-13-transactions.h"
#endif
/*
 * Ejemplo específico CoAP para respuestas separadas (SEPARATED MESSAGES).
 */
RESOURCE(separate, METHOD_GET, "test/separate", "title=\"Separate demo\"");
// Una estructura para guardar la información requerida
typedef struct application_separate_store {
    // Proporcionado por Erbium para almacenar informacion generica de peticiones tales como IP remota y el token.
    coap_separate_t request_metadata;
    // Añade campos para informacion adicional para ser guardado al finalizar 
    char buffer[16];
} application_separate_store_t;

static uint8_t separate_active = 0;
static application_separate_store_t almacen[1];

void separate_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
    /*  Ejemplo que permite solo una respuesta separada abierta.
        Para multiples, la aplicacion debe gestionar la lista de stores. */
    if (separate_active == 1){
        // Rechace una solicitud que requiera una respuesta separada con un mensaje de error.
        // Cuando el servidor no tiene suficientes recursos para almacenar la información para
        // una respuesta separada o de lo contrario no puede ejecutar el manejador de recursos,
        // esta función responderá con 5.03 Service Unavailable. El cliente puede intentarlo más tarde.
        coap_separate_reject();
    }
    // La primera vez entra por aqui, despues pone a 1 el separate y entra
    // por el if, por eso las siguientes las rechazar con error 503
    else{
        separate_active = 1;
        // Tomar el control y saltar la respuesta por el motor.
        // Iniciar una respuesta independiente con un ACK vacío.
        coap_separate_accept(request, &almacen->request_metadata);
        // Sea consciente de respetar la opción Block2, que también se almacena en el coap_separate_t

        /*  Por el momento, solo la minima informacion se guarda en el store (client address, port, token, MID, type, and Block2).
            Amplíe el store, si la aplicación requiere información adicional de este handler.
            El buffer es un ejemplo de campo para informacion personalizada. */
        snprintf(almacen->buffer, sizeof(almacen->buffer), "StoredInfo");
    }
}

void separate_finalize_handler(){
    if (separate_active == 1){
        coap_transaction_t *transaction = NULL;
        // Usa transacciones para responder a una peticion confirmable (CON)
                                                    // MID message ID                   // DIRECCION IP                 // PUERTO
        if ((transaction = coap_new_transaction(almacen->request_metadata.mid, &almacen->request_metadata.addr, almacen->request_metadata.port))){
            coap_packet_t response[1]; // De esta manera el paquete se trata como un puntero
            // Restaura la información de la SOLICITUD para la respuesta
                                            // respuesta en sí          // código
            coap_separate_resume(response, &almacen->request_metadata, REST.status.OK);
            coap_set_payload(response, almacen->buffer, strlen(almacen->buffer));
            /*  Be aware to respect the Block2 option, which is also stored in the coap_separate_t.
                As it is a critical option, this example resource pretends to handle it for compliance.*/
            coap_set_header_block2(response, almacen->request_metadata.block2_num, 0, almacen->request_metadata.block2_size);
            // Advertencia: No hay comprobación de error de serialización.
            transaction->packet_len = coap_serialize_message(response, transaction->packet);
            coap_send_transaction(transaction);
            /* The engine will clear the transaction (right after send for NON, after acked for CON). */
            // El motor borrará la transacción (justo después de enviar un NON, después de responder con CON)
            separate_active = 0;
        }else{
            /*  Set timer for retry, send error message, ...
            The example simply waits for another button press. */
        }
    }
}
#endif

PROCESS(rest_server_example, "Erbium Example Server");
AUTOSTART_PROCESSES(&rest_server_example);

PROCESS_THREAD(rest_server_example, ev, data)
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
#if REST_RES_HELLO
  	rest_activate_resource(&resource_helloworld);
#endif
#if defined (PLATFORM_HAS_LIGHT) && REST_RES_LIGHT
  	SENSORS_ACTIVATE(light_sensor);
  	rest_activate_resource(&resource_light);
#endif
#if REST_RES_CHUNKS
    rest_activate_resource(&resource_chunks);
#endif
#if REST_RES_PUSHING
    rest_activate_periodic_resource(&periodic_resource_pushing);
#endif
#if REST_RES_SUB
    rest_activate_resource(&resource_sub);
#endif
#if defined (PLATFORM_HAS_BUTTON) && REST_RES_EVENT
    rest_activate_event_resource(&resource_boton);
#endif
#if defined (PLATFORM_HAS_LEDS) && REST_RES_LEDS
    rest_activate_resource(&resource_leds);
#endif

#if defined (PLATFORM_HAS_BUTTON) && REST_RES_SEPARATE && WITH_COAP > 3
    /* No pre-handler anymore, user coap_separate_accept() and coap_separate_reject(). */
    rest_activate_resource(&resource_separate);
#endif
#if defined (PLATFORM_HAS_BUTTON) && (REST_RES_EVENT || (REST_RES_SEPARATE && WITH_COAP > 3))
  SENSORS_ACTIVATE(button_sensor);
#endif
#if REST_RES_SEPARATE && WITH_COAP>3
      /* Also call the separate response example handler. */
    separate_finalize_handler();
#endif
    while(1) {
        PROCESS_WAIT_EVENT();
        #if defined (PLATFORM_HAS_BUTTON)
            if (ev == sensors_event && data == &button_sensor) {
                PRINTF("BUTTON\n");
                #if REST_RES_EVENT
                    /* Call the event_handler for this application-specific event. */
                    boton_event_handler(&resource_boton);
                #endif

            }
        #endif /* PLATFORM_HAS_BUTTON */
    }
  	PROCESS_END();
}