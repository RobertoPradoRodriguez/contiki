#include "contiki.h"
#include "contiki-lib.h"
#include "contiki-net.h"
#include "net/uip.h"
#include "net/uip-ds6.h"
#include "net/rpl/rpl.h"

#include "net/netstack.h"
#include "dev/button-sensor.h"
#include "dev/slip.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define DEBUG DEBUG_NONE
#include "net/uip-debug.h"

uint16_t dag_id[] = {0x1111, 0x1100, 0, 0, 0, 0, 0, 0x0011};

static uip_ipaddr_t prefix;
static uint8_t prefix_set;

PROCESS(border_router_process, "Border router process"); // PROCESS CONTROL BLOCK (PCB)

AUTOSTART_PROCESSES(&border_router_process);		 // INICIA EL PROCESO AUTOMATICAMENTE

void request_prefix(void)
{
  	// ESTROPEAR ip_buf CON UNA PETICION SUCIA...
  	// uip_buf --> BUFFER donde guardas lo que vas a mandar
	uip_buf[0] = '?';
  	uip_buf[1] = 'P';
  	// uip_len --> LA LONGITUD DEL PAQUETE EN EL BUFFER uip_buf
  	uip_len = 2;
  	printf("BORDER ROUTER\n");
  	printf("buf 0 --> %c\n", uip_buf[0]);
  	printf("buf 1 --> %c\n", uip_buf[1]);
  	// slip_send() --> envia lo que haya en el buffer uip_buf 
  	slip_send();
  	uip_len = 0;
}

void set_prefix_64(uip_ipaddr_t *prefix_64)
{
  	uip_ipaddr_t ipaddr;
  	memcpy(&prefix, prefix_64, 16);
  	memcpy(&ipaddr, prefix_64, 16);
  	prefix_set = 1;
  	uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
  	uip_ds6_addr_add(&ipaddr, 0, ADDR_AUTOCONF);
}

PROCESS_THREAD(border_router_process, ev, data) // PROCESS THREAD
{
  	static struct etimer et;
  	rpl_dag_t *dag;

  	PROCESS_BEGIN();
  	/*Mientras espera que el prefijo se envíe a través de la conexión SLIP, 
	el futuro enrutador frontera puede unirse a un DAG existente como padre o hijo,
	o adquirir un enrutador predeterminado que más tarde tendrá prioridad sobre
 	la interfaz de fallo de SLIP. Impedir que al apagar la radio hasta que se 
 	inicializan como un DAG root.	*/
  	prefix_set = 0;
  	// APAGA LA RADIO
 	// Esto se hace para ahorrar energia cuando no la vas a usar  
  	NETSTACK_MAC.off(0);
	PROCESS_PAUSE();   				// PARA TEMPORALMENTE EL PROTOTHREAD
	SENSORS_ACTIVATE(button_sensor);

	// PRINTA SACA POR LA PANTALLA "Mote output" de COOJA
  	PRINTA("RPL-Border router started\n");
   	

	///// PIDE EL PREFIJO HASTA QUE LO RECIBE /////////
  	while(!prefix_set) {
    	etimer_set(&et, CLOCK_SECOND);
    	request_prefix();
    	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et)); // ESPERA POR UN EVENTO BAJO ESTA CONDICION 
  	}
  	// UNA VEZ QUE LO RECIBE, SE CONFIGURA COMO ROOT DE LA RED
  	dag = rpl_set_root(RPL_DEFAULT_INSTANCE,(uip_ip6addr_t *)dag_id);
  	if(dag != NULL) {
    	rpl_set_prefix(dag, &prefix, 64);
    	PRINTA("Created a new RPL dag\n");
  	}
	///////////////////////////////////////////////////////
 
  	NETSTACK_MAC.off(1);

  	while(1) {
    	PROCESS_YIELD();  				// ESPERA POR CUALQUIER EVENTO
    	if (ev == sensors_event && data == &button_sensor) {
    	  	PRINTF("Initiating global repair\n");
      		rpl_repair_root(RPL_DEFAULT_INSTANCE);
    	}
  	}
  	PROCESS_END();  				// ACABA EL PROTOTHREAD
}