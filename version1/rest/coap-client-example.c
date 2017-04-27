#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "contiki.h"
#include "contiki-net.h"
#include "rest.h"
#include "buffer.h"

#define DEBUG 1
#if DEBUG
	#include <stdio.h>
	#define PRINTF(...) printf(__VA_ARGS__)
	#define PRINT6ADDR(addr) PRINTF("%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x", ((uint8_t *)addr)[0], ((uint8_t *)addr)[1], ((uint8_t *)addr)[2], ((uint8_t *)addr)[3],((uint8_t *)addr)[4], ((uint8_t *)addr)[5], ((uint8_t *)addr)[6], ((uint8_t *)addr)[7],((uint8_t *)addr)[8], ((uint8_t *)addr)[9], ((uint8_t *)addr)[10], ((uint8_t *)addr)[11],((uint8_t *)addr)[12], ((uint8_t *)addr)[13], ((uint8_t *)addr)[14], ((uint8_t *)addr)[15])

	#define PRINTLLADDR(lladdr) PRINTF(" %02x:%02x:%02x:%02x:%02x:%02x ",(lladdr)->addr[0], (lladdr)->addr[1],(lladdr)->addr[2], (lladdr)->addr[3],(lladdr)->addr[4], (lladdr)->addr[5])
#else
	#define PRINTF(...)
	#define PRINT6ADDR(addr)
	#define PRINTLLADDR(addr)
#endif

#define SERVER_NODE(ipaddr) uip_ip6addr(ipaddr, 0xfe80, 0, 0, 0, 0x0212, 0x7401, 0x0001, 0x0101)
#define LOCAL_PORT 61617
#define REMOTE_PORT 61616

char temp[100];
int xact_id;
static uip_ipaddr_t server_ipaddr;
static struct uip_udp_conn *client_conn;
static struct etimer et;
#define MAX_PAYLOAD_LEN   100

#define NUMBER_OF_URLS 3
char* service_urls[NUMBER_OF_URLS] = {"light", ".well-known/core", "helloworld"};

static void response_handler(coap_packet_t* response)
{
  	uint16_t payload_len = 0;
  	uint8_t* payload = NULL;
  	payload_len = coap_get_payload(response, &payload);

  	PRINTF("Response transaction id: %u", response->tid);
  	if (payload){
  		// Copia los primeros PAYLOAD_LEN caracteres del 
  		// objeto apuntado por PAYLOAD al objeto apuntado por TEMP.
    	memcpy(temp, payload, payload_len);
    	temp[payload_len] = 0;
    	PRINTF(" payload: %s\n", temp);
  	}
}

static void send_data(void)
{
  	char buf[MAX_PAYLOAD_LEN];
    ////////////////////////////////////////////////
    // TODO VIENE EN apps/rest-coap/coap-server.h //
    ////////////////////////////// /coap-common.h //
    ///////////////////  /rest-common/buffer.h    //
    ////////////////////////////////////////////////
    // Inicializa el buffer --> Llama primero a delete_buffer()
    // y luego inicializa el nuevo buffer (malloc) con el tamaño pasado
    // por parametro
  	if (init_buffer(COAP_DATA_BUFF_SIZE)) {
    	int data_size = 0;
    	// DEFINE UN PAQUETE PARA MANDAR UNA PETICION COAP
    	coap_packet_t* request = (coap_packet_t*)allocate_buffer(sizeof(coap_packet_t));
    	// Inicializa todos los campos de un paquete COAP
      init_packet(request);
    	// DEFINE EL METODO DE LA PETICION HTTP
    	coap_set_method(request, COAP_GET);
    	// ID del paquete COAP
      request->tid = xact_id++;
      // Tipo de mensaje COAP, hay 4 tipos (CON,NON,ACK,RST)
    	request->type = MESSAGE_TYPE_CON;
    	// ESTABLECE LA URI DE LA PETICION
    	int service_id = random_rand() % NUMBER_OF_URLS;
    	coap_set_header_uri(request, service_urls[service_id]);

    	data_size = serialize_packet(request, buf);
    	PRINTF("Client sending request to:[");
    	// ESTA DECLARADO COMO UNA CONSTANTE ARRIBA: 
    	// IMPRIME LA DIRECCION IP EN UN FORMATO BONITO
    	PRINT6ADDR(&client_conn->ripaddr);
    	PRINTF("]:%u/%s\n", (uint16_t)REMOTE_PORT, service_urls[service_id]);
    	// MANDA UN PAQUETE UDP CON LOS DATOS DEL BUFFER 
    	uip_udp_packet_send(client_conn, buf, data_size);
      // Libera la memoria del buffer, haciendo que el buffer
      // apunte a NULL y poniendo su tamaño a 0
    	delete_buffer();
  }
}

static void handle_incoming_data()
{
  	PRINTF("Incoming packet size: %u \n", (uint16_t)uip_datalen());
  	if (init_buffer(COAP_DATA_BUFF_SIZE)) {
    	// La funcion uip_newdata mira si hay datos nuevos disponibles
  		// Si la hay, estara en el puntero uip_appdata y su tamaño
  		// en la variable uip_len
    	if(uip_newdata()) {
      		coap_packet_t* response = (coap_packet_t*)allocate_buffer(sizeof(coap_packet_t));
      		if(response) {
        		parse_message(response, uip_appdata, uip_datalen());
        		response_handler(response);
      		}
    	}
    	delete_buffer();
  	}
}

PROCESS(coap_client_example, "COAP Client Example");	// PCB
AUTOSTART_PROCESSES(&coap_client_example);				// AUTOINICIA EL PROCESO 

PROCESS_THREAD(coap_client_example, ev, data)			// PROCESS THREAD
{
  	PROCESS_BEGIN();									// INICIA EL PROTOTHREAD

  	// definido arriba --> guarda en server_ipaddr la IP de servidor
  	SERVER_NODE(&server_ipaddr);

  	// NUEVA CONEXION CON EL SERVIDOR
  	client_conn = udp_new(&server_ipaddr, UIP_HTONS(REMOTE_PORT), NULL);
  	// Enlaza la conexión al puerto local del cliente
  	udp_bind(client_conn, UIP_HTONS(LOCAL_PORT));

  	PRINTF("Created a connection with the server ");
  	PRINT6ADDR(&client_conn->ripaddr);
  	PRINTF(" local/remote port %u/%u\n", UIP_HTONS(client_conn->lport), UIP_HTONS(client_conn->rport));
  	// CUANDO EL TIEMPO ACABA, SE MANDA UN EVENTO PROCESS_EVENT_TIMER 
  	// (RESERVADO POR EL KERNEL) AL PROCESO QUE LLAMO A LA FUNCION,
  	// ES DECIR, SE LO MANDA A SÍ MISMO
  	etimer_set(&et, 5 * CLOCK_SECOND);
  	while(1) {
    	PROCESS_YIELD();								// ESPERA POR CUALQUIER EVENTO
    	// SI EXPIRO EL EVENTO TIMER ANTERIOR...
    	if (etimer_expired(&et)) {
      		send_data();
    		// RESETEA EL EVENTO CON EL MISMO INTERVALO DE LA VEZ ANTERIOR
      		etimer_reset(&et);
    		// SI NO, SI ES UN EVENTO TCP/IP...
    	}else if (ev == tcpip_event) {
      		handle_incoming_data();
    	}
  	}
  	PROCESS_END();										// ACABA EL PROTOTHREAD
}