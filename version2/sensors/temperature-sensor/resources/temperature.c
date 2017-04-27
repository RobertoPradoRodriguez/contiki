#include "contiki.h"

#if PLATFORM_HAS_SHT11
    #include <string.h>
    #include "rest-engine.h"
    #include "dev/sht11/sht11-sensor.h"

    static void temperature_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);

	//		  /nombre/ 												   /GET/	    /POST/ /PUT/ /DELETE/
    RESOURCE(temperature, "title=\"Temperature\";rt=\"Sht11\"", temperature_handler, NULL, NULL,  NULL);

    static void temperature_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
        
        uint16_t temp = ((sht11_sensor.value(SHT11_SENSOR_TEMP) / 10) - 396) / 10;
        //uint16_t temp = sht11_sensor.value(SHT11_SENSOR_TEMP);
        REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
        snprintf((char *)buffer, REST_MAX_CHUNK_SIZE, "Temperature: %u ºC", temp);
        REST.set_response_payload(response, (uint8_t *)buffer, strlen((char *)buffer));
    }
#endif