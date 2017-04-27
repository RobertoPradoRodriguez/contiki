#include "contiki.h"
#include "contiki-lib.h"
#include "contiki-net.h"

#include "net/ip/uip.h"
#include "net/rpl/rpl.h"

#include "net/netstack.h"
#include "dev/button-sensor.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#define DEBUG DEBUG_PRINT
#include "net/ip/uip-debug.h"

#define UIP_IP_BUF   ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])

#define UDP_CLIENT_PORT	8765
#define UDP_SERVER_PORT	5678

#define UDP_EXAMPLE_ID  190

// Este código hace básicamente tres cosas:
//	1. Inicializar el DAG RPL
//	2. Configurar conexion UDP
//	3. Esperar por paquetes del cliente e imprimirlos	

static struct uip_udp_conn *server_conn;

PROCESS(udp_server_process, "UDP server process");
AUTOSTART_PROCESSES(&udp_server_process);

static void tcpip_handler(void)
{
  	char *appdata;
  	// La funcion uip_newdata mira si hay datos nuevos disponibles
  	// Si la hay, estara en el puntero uip_appdata y su tamaño
  	// en la variable uip_len
  	if(uip_newdata()) {
    	appdata = (char *)uip_appdata;
    	appdata[uip_datalen()] = 0;
    	PRINTF("DATA recv '%s' from ", appdata);
    	// UIP_IP_BUF --> puntero a la cabecera ip
    	PRINTF("%d", UIP_IP_BUF->srcipaddr.u8[sizeof(UIP_IP_BUF->srcipaddr.u8) - 1]);
    	PRINTF("\n");
#if SERVER_REPLY
    	PRINTF("DATA sending reply\n");
    	// uip_ipaddr_copy --> Copia una direccion IP a otra dir. IP 
    	uip_ipaddr_copy(&server_conn->ripaddr, &UIP_IP_BUF->srcipaddr);
    	// manda datos (la palabra "Reply") indicando su tamaño por una
    	// conexión establecida
    	uip_udp_packet_send(server_conn, "Reply", sizeof("Reply"));
		// Configura una IP sin especificar (como ponerla a ceros)    
		uip_create_unspecified(&server_conn->ripaddr);
#endif
  }
}

static void print_local_addresses(void)
{
  	int i;
  	uint8_t state;

 	PRINTF("Server IPv6 addresses: ");
  	for(i = 0; i < UIP_DS6_ADDR_NB; i++) {
  		// uip_ds6_if --> PARAMETROS DE LA INTERFAZ
    	state = uip_ds6_if.addr_list[i].state;
    	// POSIBLES ESTADOS DE UNA DIRECCION
    		// #define ADDR_TENTATIVE 0
    		// #define ADDR_PREFERRED 1
    		// #define ADDR_DEPRECATED 2
    	if(state == ADDR_TENTATIVE || state == ADDR_PREFERRED) {
      		PRINT6ADDR(&uip_ds6_if.addr_list[i].ipaddr);
      		PRINTF("\n");
      		if (state == ADDR_TENTATIVE) {
        		uip_ds6_if.addr_list[i].state = ADDR_PREFERRED;
      		}
    	}
  	}
}

PROCESS_THREAD(udp_server_process, ev, data)
{
	  uip_ipaddr_t ipaddr;
  	struct uip_ds6_addr *root_if;

  	PROCESS_BEGIN();
	  PROCESS_PAUSE();

  	// ACTIVA EL SENSOR DE BOTON (HAY MAS SENSORES)
  	SENSORS_ACTIVATE(button_sensor);
	PRINTF("UDP server started\n");

#if UIP_CONF_ROUTER
	//////// FUNCION uip_ip6addr --> CONSTRUYE UNA IPv6 //////////////
	#if 0
   		uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 1);
	#elif 1
  		uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0x00ff, 0xfe00, 1);
	#else
  		uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
  		uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
	#endif
////////////////////////////////////////////////////////////////
/////// 1. COMPRUEBA SI LA IP SE CONFIGURO BIEN O NO ////////
 							// Tipo de configuracion de la ip:
 							// #define  ADDR_ANYTYPE 0
							// #define  ADDR_AUTOCONF 1
							// #define  ADDR_DHCP 2
							// #define  ADDR_MANUAL 3
  	uip_ds6_addr_add(&ipaddr, 0, ADDR_MANUAL);
  	root_if = uip_ds6_addr_lookup(&ipaddr);
  	if(root_if != NULL) {
    	rpl_dag_t *dag;
    	dag = rpl_set_root(RPL_DEFAULT_INSTANCE,(uip_ip6addr_t *)&ipaddr);
    	uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
    	rpl_set_prefix(dag, &ipaddr, 64);
    	PRINTF("created a new RPL dag\n");
  	}else {
    	PRINTF("failed to create a new RPL DAG\n");
  	}
/////////////////////////////////////////////////////////////////
#endif // ACABA EL IF DE UIP_CONF_ROUTER 
  
  	print_local_addresses();
	NETSTACK_MAC.off(1);

////// 2. CREA UNA CONEXION CON EL PUERTO DEL CLIENTE/////////////
	// VEMOS QUE CREA UNA CONEXION A UNA IP NULA
  					            //IP cliente/ 	  /PUERTO cliente/      
  	server_conn = udp_new(  NULL, 	UIP_HTONS(UDP_CLIENT_PORT), NULL);
  	if(server_conn == NULL) {
    	PRINTF("No UDP connection available, exiting the process!\n");
    	PROCESS_EXIT();
  	}
  	// Enlaza la conexión al puerto local del servidor
  	udp_bind(server_conn, UIP_HTONS(UDP_SERVER_PORT));

  	PRINTF("Created a server connection with remote address ");
  	PRINT6ADDR(&server_conn->ripaddr);
  	PRINTF(" local/remote port %u/%u\n", UIP_HTONS(server_conn->lport),
                                         UIP_HTONS(server_conn->rport));
///////////////////////////////////////////////////////////////
/////// 3. RECIBE PAQUETES ENTRANTES Y LOS PROCESA/////////////
	while(1) {
    	PROCESS_YIELD();
		  // SI HAY UN PAQUETE DISPONIBLE...
    	if(ev == tcpip_event) {
	  		  // ...LLAMA A ESTA FUNCION
      		tcpip_handler();
    	}else if (ev == sensors_event && data == &button_sensor) {
      		PRINTF("Initiaing global repair\n");
      		rpl_repair_root(RPL_DEFAULT_INSTANCE);
    	}
  	}

  PROCESS_END();
}