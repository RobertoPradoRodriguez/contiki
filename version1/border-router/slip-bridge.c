#include "net/uip.h"
#include "net/uip-ds6.h"
#include "dev/slip.h"
#include "dev/uart1.h"
#include <string.h>
/*  UIP_LLH_LEN --> La longitud de la cabecera del nivel de enlace. 
    Este es el desplazamiento en el uip_buf donde se puede
    encontrar la cabecera IP. Para Ethernet, esto debería
    establecerse en 14. Para SLIP, esto debería establecerse en 0.
*/
#define UIP_IP_BUF  ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])

#define DEBUG DEBUG_PRINT
#include "net/uip-debug.h"

void set_prefix_64(uip_ipaddr_t *);
static uip_ipaddr_t last_sender;
/*
    SLIP -> Serial Line IP (protocolo CAPA DE ENLACE)
    El protocolo SLIP es una forma muy sencilla
    de transmitir paquetes IP a través de una línea 
    serie. No proporciona ningún enmarcado o control
    de errores, y por lo tanto no es muy utilizado 
    hoy en día. En PC, SLIP se ha sustituido por el PPP 
    (Point-to-Point Protocol) cuyo diseño es superior,
    tiene más y mejores características y no requiere 
    de la configuración de su dirección IP antes de 
    ser establecido. Sin embargo, con MICROCONTROLADORES,
    se sigue utilizando el modo de encapsulación de SLIP 
    para paquetes IP ya que usa cabeceras de tamaño reducido.
*/
static void slip_input_callback(void)
{
    if(uip_buf[0] == '!') {
        PRINTF("Got configuration message of type %c\n", uip_buf[1]);
        uip_len = 0;
        if(uip_buf[1] == 'P') {
            uip_ipaddr_t prefix;
            // Configuramos el prefijo
            // Copia el valor 0 (convertido a unsigned char --> 1 byte)
            // en cada uno de los PRIMEROS 16 caracteres en el 
            // objeto apuntado por prefix.
            // RESUMEN --> PONE LA IP TODO A CEROS (16x8=128) 
            memset(&prefix, 0, 16);
            // Copia los primeros 8 caracteres del objeto 
            // apuntado por uip_buf[2] al objeto apuntado por prefix.
            memcpy(&prefix, &uip_buf[2], 8);
            PRINTF("Setting prefix ");
            PRINT6ADDR(&prefix);
            PRINTF("\n");
            set_prefix_64(&prefix);
        }
    }else if (uip_buf[0] == '?') {
        PRINTF("Got request message of type %c\n", uip_buf[1]);
        if(uip_buf[1] == 'M') {
            char* hexchar = "0123456789abcdef";
            int j;
            /* this is just a test so far... just to see if it works */
            uip_buf[0] = '!';
            for(j = 0; j < 8; j++) {
                uip_buf[2 + j * 2] = hexchar[uip_lladdr.addr[j] >> 4];
                uip_buf[3 + j * 2] = hexchar[uip_lladdr.addr[j] & 15];
            }
            uip_len = 18;
            // slip_send() --> envia lo que haya en el buffer uip_buf
            slip_send();
        }
    	uip_len = 0;
    }
    /* Guarde el último REMITENTE recibido en SLIP para evitar rebotar
     el paquete si no se encuentra ninguna ruta */
                    // destino          /origen/
    uip_ipaddr_copy(&last_sender, &UIP_IP_BUF->srcipaddr);
}

static void init(void)
{
    // Inicializa el controlador SLIP específico de la arquitectura
    slip_arch_init(BAUD2UBR(115200));
    process_start(&slip_process, NULL);		// INICIA UN PROCESO Y LE PASA UN PUNTERO A NULL
    // Establece una función a ser llamada cuando hay actividad en la interfaz SLIP
    // Usado para detectar si un nodo es puerta de enlace.
    slip_set_input_callback(slip_input_callback);
}

static void output(void)
{
    // SI LAS DOS IPs SON IGUALES...
    if(uip_ipaddr_cmp(&last_sender, &UIP_IP_BUF->srcipaddr)) {
        // No devolver los paquetes de nuevo sobre SLIP
        // si el paquete se recibió a través de SLIP
        PRINTF("slip-bridge: Destination off-link but no route src=");
        PRINT6ADDR(&UIP_IP_BUF->srcipaddr);
        PRINTF(" dst=");
        PRINT6ADDR(&UIP_IP_BUF->destipaddr);
        PRINTF("\n");
    }else {
        // PRINTF("SUT: %u\n", uip_len);
        // slip_send() --> envia lo que haya en el buffer uip_buf 
        slip_send();
    }
}

#if !SLIP_BRIDGE_CONF_NO_PUTCHAR
#undef putchar
int putchar(int c)
{
#define SLIP_END    0300
    static char debug_frame = 0;

    if(!debug_frame) {            /* Start of debug output */
        // Escribe un byte sobre SLIP
        slip_arch_writeb(SLIP_END);
        slip_arch_writeb('\r');     /* Type debug line == '\r' */
        debug_frame = 1;
    }

    /* Need to also print '\n' because for example COOJA will not show
    any output before line end */
    slip_arch_writeb((char)c);

    /*
    * Line buffered output, a newline marks the end of debug output and
    * implicitly flushes debug output.
    */
    if(c == '\n') {
        slip_arch_writeb(SLIP_END);
        debug_frame = 0;
    }
    return c;
}
#endif

/*  ACTUALMENTE HAY DOS TIPOS DE INTERFACES:

    IF_RADIO - enlace radio (o PLC) con 6lowpan y RPL
    IF_FALLBACK - enlace Ethernet con IPv6 ND y Router Advertisements

    --> TODAS TIENEN UN METODO INIT Y OUTPUT */

const struct uip_fallback_interface rpl_interface = {
  init, output
};