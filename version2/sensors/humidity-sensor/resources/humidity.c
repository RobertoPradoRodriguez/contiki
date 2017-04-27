#include "contiki.h"

#if PLATFORM_HAS_SHT11
	#include <string.h>
	#include "rest-engine.h"
	#include "dev/sht11/sht11-sensor.h"

	static void humidity_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);

	//		 /nombre/ 									  	   /GET/	   /POST/ /PUT/ /DELETE/
	RESOURCE(humidity, "title=\"Humidity\";rt=\"Sht11\"", humidity_handler, NULL, NULL,  NULL);

	static void humidity_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){

		uint16_t val = sht11_sensor.value(SHT11_SENSOR_HUMIDITY);
	    int calibrar = (val*405/10000) - 4;
	    calibrar += (-28 / 10000000) * val * val;
	    REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
	    snprintf((char *)buffer, REST_MAX_CHUNK_SIZE, "Humidity: %u %%", val);
	    REST.set_response_payload(response, (uint8_t *)buffer, strlen((char *)buffer));
	}
#endif 
