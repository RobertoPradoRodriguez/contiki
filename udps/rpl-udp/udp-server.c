#include "contiki.h"
#include "contiki-lib.h"
#include "contiki-net.h"

#include "net/ip/uip.h"
#include "net/rpl/rpl.h"

#include "net/netstack.h"
#include "dev/button-sensor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define DEBUG DEBUG_PRINT
#include "net/ip/uip-debug.h"

PROCESS(udp_server_process, "UDP server process");
AUTOSTART_PROCESSES(&udp_server_process);

PROCESS_THREAD(udp_server_process, ev, data){

    uip_ip6addr_t ipaddr;
    uip_ip6addr_t root;
    struct uip_ds6_addr *root_if;

    PROCESS_BEGIN();
    PRINTF("RPL dag started!\n");

#if UIP_CONF_ROUTER
    ////////////////////////  CONFIGURAMOS LAS DOS PARTES DE NUESTRA IPv6  /////////////////////////////
    // configurar mi dir IPv6																		  //
    uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0); 	// --> AQUI ESTABLECEMOS LOS 64 PRIMEROS BITS //
    // establece los ultimos 64 bits de una IP basándose en la MAC 									  //
    //                   /dir IP/  /dir MAC/														  //
    uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);		  	// --> AQUI ESTABLECEMOS LOS 64 ÚLTIMOS BITS  //
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    /////// 1. COMPRUEBA SI LA IP SE CONFIGURO BIEN O NO ////////
                // Tipo de configuracion de la ip:
                // #define  ADDR_ANYTYPE 0
                // #define  ADDR_AUTOCONF 1
                // #define  ADDR_DHCP 2
                // #define  ADDR_MANUAL 3
    //               /dir IP/ /validez/  /tipo conf/
    uip_ds6_addr_add(&ipaddr,     0,    ADDR_AUTOCONF);
    // creamos la ip del root
    uip_ip6addr(&root, 0xaaaa, 0, 0, 0, 0x0212, 0x7401, 0x0001, 0x0101);
    root_if = uip_ds6_addr_lookup(&root);
    if(root_if != NULL) {
        rpl_dag_t *dag;
        //                   /RPL INSTANCE ID/   /dag id --> dir IP root/
        dag = rpl_set_root(RPL_DEFAULT_INSTANCE,(uip_ip6addr_t *)&root);
        // ponemos la dir IP TODO a CEROS (no se para que lo hace)
        //uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
        // Como soy el NODO ROOT, estabezco el prefijo
        //                  /dir ip/ /len/ 
        //rpl_set_prefix(dag, &ipaddr,  64);
        PRINTF("Created a new RPL dag\n");
    }else{
        PRINTF("Failed to create a new RPL DAG\n");
    }
#endif
    /*  RPL BÁSICO:
        
        - DODAGVersionNumber: Es un contador secuencial que es incrementado 
        por el root para formar una nueva versión de un DODAG. Una versión 
        de DODAG es identificada de forma unívoca por la tupla 
        (RPLInstanceID, DODAGID, DODAGVersionNumber).
        
        - RPL Instance: Un conjunto de DODAGs, posiblemente múltiples. 
        Una red puede tener más de una instancia RPL y un nodo RPL puede
        participar en múltiples instancias RPL. Cada Instancia RPL funciona
        independientemente de otras Instancias RPL. Este documento describe
        el funcionamiento dentro de una sola instancia de RPL. En RPL,
        un nodo puede pertenecer como máximo a un DODAG por instancia RPL.
        La tupla (RPLInstanceID, DODAGID) identifica de forma única a un DODAG.
        
        - RPLInstanceID: Es un identificador único dentro de una red. 
        DODAGs con el mismo RPLInstanceID comparten la misma OF. 
        
        - DODAGID: Es un identificador del DODAG root. El DODAGID es
        único dentro del alcance de un RPL Instance en la  LLN. La
        tupla (RPLInstanceID, DODAGID) identifica unívocamente un DODAG.
    
		- OF-Función objetivo. Define qué métricas de enrutamiento, 
		objetivos de optimización y funciones relacionadas se utilizan
		en un DODAG.

		- Rank: El rank de un nodo en un DAG identifica la posición de 
		los nodos con respecto a una raíz DODAG. Cuanto más lejos esté 
		un nodo de una raíz DODAG, más alto es el rank de ese nodo. 
		El rank de un nodo puede ser una distancia topológica simple, 
		o puede ser más comúnmente calculado como una función de otras
		propiedades como se describe más adelante.

		Si somos EL ROOT y sólo necesitamos eliminar una ruta DAO,
        lo más probable es que la red necesita ser REPARADA. La función
        rpl_repair_root() causará una reparación global SI SOMOS EL
        NODO ROOT del dag.
        Un root DODAG establece una operación de reparación global
        INCREMENTANDO el número de DODAGVersion. Esto inicia una nueva 
        versión de DODAG. Los nodos de la nueva versión de DODAG pueden 
        elegir una nueva posición cuyo RANK no está limitado por su 
        rango dentro de la antigua versión de DODAG.
            
        FUNCION QUE HACE ESTO:  rpl_repair_root(RPL_DEFAULT_INSTANCE);

    */
    PROCESS_END();
}