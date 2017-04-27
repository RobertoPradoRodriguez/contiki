#include "contiki.h"

#if PLATFORM_HAS_LIGHT
	#include <string.h>
	#include "rest-engine.h"
	#include "dev/light-sensor.h"

	static void light_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);

	//		/nombre/ 																   /GET/	 /POST/ /PUT/ /DELETE/
	RESOURCE(light, "title=\"Photosynthetic and solar light\";rt=\"LightSensor\"", light_handler, NULL, NULL,  NULL);

	static void light_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
		
	    uint16_t fotosintetica = 40 * light_sensor.value(LIGHT_SENSOR_PHOTOSYNTHETIC) / 98;
	    uint16_t solar = 40 * light_sensor.value(LIGHT_SENSOR_TOTAL_SOLAR) / 98;
	    REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
	    snprintf((char *)buffer, REST_MAX_CHUNK_SIZE, "Photosynthetic: %u lux\nSolar: %u lux", fotosintetica, solar);
	    REST.set_response_payload(response, (uint8_t *)buffer, strlen((char *)buffer));
	}
#endif
