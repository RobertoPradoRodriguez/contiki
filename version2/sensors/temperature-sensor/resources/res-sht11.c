#include "contiki.h"

#if PLATFORM_HAS_SHT11

#include <string.h>
#include "rest-engine.h"
#include "dev/sht11/sht11-sensor.h"

static void res_get_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);

/* Get Method Example. Returns the reading from temperature and humidity sensors. */
RESOURCE(res_sht11,
         "title=\"Temperature and Humidity\";rt=\"Sht11\"",
         res_get_handler,
         NULL,
         NULL,
         NULL);

static void
res_get_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  /* Temperature in Celsius (t in 14 bits resolution at 3 Volts)
   * T = -39.60 + 0.01*t
   */
  uint16_t temperature = ((sht11_sensor.value(SHT11_SENSOR_TEMP) / 10) - 396) / 10;
  /* Relative Humidity in percent (h in 12 bits resolution)
   * RH = -4 + 0.0405*h - 2.8e-6*(h*h)
   */
  uint16_t rh = sht11_sensor.value(SHT11_SENSOR_HUMIDITY);

  unsigned int accept = -1;
  REST.get_header_accept(request, &accept);

  if(accept == -1 || accept == REST.type.TEXT_PLAIN) {
    REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
    snprintf((char *)buffer, REST_MAX_CHUNK_SIZE, "%u;%u", temperature, rh);

    REST.set_response_payload(response, (uint8_t *)buffer, strlen((char *)buffer));
  } else if(accept == REST.type.APPLICATION_XML) {
    REST.set_header_content_type(response, REST.type.APPLICATION_XML);
    snprintf((char *)buffer, REST_MAX_CHUNK_SIZE, "<Temperature =\"%u\" Humidity=\"%u\"/>", temperature, rh);

    REST.set_response_payload(response, buffer, strlen((char *)buffer));
  } else if(accept == REST.type.APPLICATION_JSON) {
    REST.set_header_content_type(response, REST.type.APPLICATION_JSON);
    snprintf((char *)buffer, REST_MAX_CHUNK_SIZE, "{'Sht11':{'Temperature':%u,'Humidity':%u}}", temperature, rh);

    REST.set_response_payload(response, buffer, strlen((char *)buffer));
  } else {
    REST.set_response_status(response, REST.status.NOT_ACCEPTABLE);
    const char *msg = "Supporting content-types text/plain, application/xml, and application/json";
    REST.set_response_payload(response, msg, strlen(msg));
  }
}
#endif /* PLATFORM_HAS_SHT11 */
