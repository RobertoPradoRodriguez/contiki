#include <stdio.h>
#include <stdlib.h>
#include "contiki.h"
#include "contiki-net.h"
#include "contiki-conf.h"
#include "mqtt.h"
#include "net/ip/uip.h"
#include "net/ipv6/uip-icmp6.h"
#include "net/ipv6/sicslowpan.h"
#include "sys/etimer.h"
#include "sys/ctimer.h"
#include "lib/sensors.h"
#include "dev/button-sensor.h"
#include "dev/temperature-sensor.h"
#include "dev/leds.h"

#include <string.h>
/* IBM server: quickstart.messaging.internetofthings.ibmcloud.com
  (184.172.124.189) mapped in an NAT64 (prefix 64:ff9b::/96) IPv6 address
  Note: If not able to connect; lookup the IP address again as it may change.
 
  Alternatively, publish to a local MQTT broker (e.g. mosquitto) running on
  the node that hosts your border router */

#ifdef MQTT_DEMO_BROKER_IP_ADDR
static const char *broker_ip = MQTT_DEMO_BROKER_IP_ADDR;
#define DEFAULT_ORG_ID              "mqtt-demo"
#else
static const char *broker_ip = "0064:ff9b:0000:0000:0000:0000:b8ac:7cbd";
#define DEFAULT_ORG_ID         "quickstart"
#endif
/* A timeout used when waiting for something to happen (e.g. to connect or to
 disconnect) */

#define STATE_MACHINE_PERIODIC     (CLOCK_SECOND >> 1)
/* Provide visible feedback via LEDS during various states */
/* When connecting to broker */
#define CONNECTING_LED_DURATION    (CLOCK_SECOND >> 2)

/* Each time we try to publish */
#define PUBLISH_LED_ON_DURATION    (CLOCK_SECOND)
/* Connections and reconnections */
#define RETRY_FOREVER              0xFF
#define RECONNECT_INTERVAL         (CLOCK_SECOND * 2)
/*
 Numero de veces que intentas reconectar con el broker.
 Puede ser un numero limitad (e.g. 3, 10 etc) o  puede establecerse a RETRY_FOREVER
 */
#define RECONNECT_ATTEMPTS         RETRY_FOREVER
#define CONNECTION_STABLE_TIME     (CLOCK_SECOND * 5)
static struct timer connection_life;
static uint8_t connect_attempt;

/* Various states */
static uint8_t state;
#define STATE_INIT            0
#define STATE_REGISTERED      1
#define STATE_CONNECTING      2
#define STATE_CONNECTED       3
#define STATE_PUBLISHING      4
#define STATE_DISCONNECTED    5
#define STATE_NEWCONFIG       6
#define STATE_CONFIG_ERROR 0xFE
#define STATE_ERROR        0xFF
/*---------------------------------------------------------------------------*/
#define CONFIG_ORG_ID_LEN        32
#define CONFIG_TYPE_ID_LEN       32
#define CONFIG_AUTH_TOKEN_LEN    32
#define CONFIG_EVENT_TYPE_ID_LEN 32
#define CONFIG_CMD_TYPE_LEN       8
#define CONFIG_IP_ADDR_STR_LEN   64

#define RSSI_MEASURE_INTERVAL_MAX 86400 /* segundos de un dia */
#define RSSI_MEASURE_INTERVAL_MIN     5 /* seg */
#define PUBLISH_INTERVAL_MAX      86400 /* segundos de un dia */
#define PUBLISH_INTERVAL_MIN          5 /* seg */

/* A timeout used when waiting to connect to a network */
#define NET_CONNECT_PERIODIC        CLOCK_SECOND
#define NO_NET_LED_DURATION         (NET_CONNECT_PERIODIC >> 1)

/* Valores de conf. por defecto */
#define DEFAULT_TYPE_ID             "nrf52dk"
#define DEFAULT_AUTH_TOKEN          "AUTHZ"
#define DEFAULT_EVENT_TYPE_ID       "status"
#define DEFAULT_SUBSCRIBE_CMD_TYPE  "+"
#define DEFAULT_BROKER_PORT         1883
#define DEFAULT_PUBLISH_INTERVAL    (30 * CLOCK_SECOND)
#define DEFAULT_KEEP_ALIVE_TIMER    60
#define DEFAULT_RSSI_MEAS_INTERVAL  (30 * CLOCK_SECOND)

/* Payload length of ICMPv6 echo requests used to measure RSSI with def rt */
#define ECHO_REQ_PAYLOAD_LEN   20

PROCESS_NAME(mqtt_demo_process);
AUTOSTART_PROCESSES(&mqtt_demo_process);
//Breve declaración de estructura de datos para la connf. del cliente MQTT
typedef struct mqtt_client_config {
	char org_id[CONFIG_ORG_ID_LEN];
	char type_id[CONFIG_TYPE_ID_LEN];
	char auth_token[CONFIG_AUTH_TOKEN_LEN];
	char event_type_id[CONFIG_EVENT_TYPE_ID_LEN];
	char broker_ip[CONFIG_IP_ADDR_STR_LEN];
	char cmd_type[CONFIG_CMD_TYPE_LEN];
	clock_time_t pub_interval;
	int def_rt_ping_interval;
	uint16_t broker_port;
} mqtt_client_config_t;
/*---------------------------------------------------------------------------*/
/* Maximum TCP segment size for outgoing segments of our socket */
#define MAX_TCP_SEGMENT_SIZE    32
/*
 * Buffers for Client ID and Topic.
 * Make sure they are large enough to hold the entire respective string
 *
 * d:quickstart:status:EUI64 is 32 bytes long
 * iot-2/evt/status/fmt/json is 25 bytes
 * We also need space for the null termination
 */
#define BUFFER_SIZE 64
static char client_id[BUFFER_SIZE];
static char pub_topic[BUFFER_SIZE];
static char sub_topic[BUFFER_SIZE];
/*
 * The main MQTT buffers.
 * We will need to increase if we start publishing more data.
 */
#define APP_BUFFER_SIZE 128
static struct mqtt_connection conn;
static char app_buffer[APP_BUFFER_SIZE];

static struct mqtt_message *mensaje_mqtt = 0;
static struct etimer publish_periodic_timer;
static struct ctimer ct;
static char *buf_ptr;
static uint16_t seq_nr_value = 0;
/* Parent RSSI functionality */
static struct uip_icmp6_echo_reply_notification echo_reply_notification;
static struct etimer echo_request_timer;
static int def_rt_rssi = 0;

static mqtt_client_config_t conf;

PROCESS(mqtt_demo_process, "MQTT Demo");

int ipaddr_sprintf(char *buf, uint8_t buf_len, const uip_ipaddr_t *addr){

  	uint16_t a;
  	uint8_t len = 0;
  	int i, f;
  	for(i = 0, f = 0; i < sizeof(uip_ipaddr_t); i += 2) {
    	a = (addr->u8[i] << 8) + addr->u8[i + 1];
    	if(a == 0 && f >= 0) {
      		if(f++ == 0) {
        		len += snprintf(&buf[len], buf_len - len, "::");
      		}
    	}else {
      		if(f > 0) {
        		f = -1;
      		}else if(i > 0) {
        			len += snprintf(&buf[len], buf_len - len, ":");
      		}
      		len += snprintf(&buf[len], buf_len - len, "%x", a);
    	}
  	}
  	return len;
}

static void echo_reply_handler(uip_ipaddr_t *source, uint8_t ttl, uint8_t *data, uint16_t datalen){
	// compara dos direcciones IPv6
							  // DEVUELVE LA IP DE NUESTRO ROUTER POR DEFECTO,
							  // ES DECIR, LA IP DE NUESTRO NODO PADRE RPL
  	if(uip_ip6addr_cmp(source, uip_ds6_defrt_choose())) {
  					// Guarda el RSSI del paquete entrante en la variable "last_rssi" 
  					// para el caso de que la capa superior quiera consultarnos posteriormente.
  					// Esa funcion solo hace un get sobre esta variable "last_rssi"
    	def_rt_rssi = sicslowpan_get_last_rssi();
  	}
}

static void publish_led_off(void *d){

  	leds_off(LEDS_GREEN);
}

static void pub_handler(const char *topic, uint16_t topic_len, const uint8_t *chunk,
            uint16_t chunk_len){

  	DBG("Pub Handler: topic='%s' (len=%u), chunk_len=%u\n", topic, topic_len, chunk_len);

  	// Si no nos gusta la longitud, ignoramos
  	if(topic_len != 23 || chunk_len != 1) {
    	printf("Incorrect topic or chunk len. Ignored\n");
    	return;
  	}
  	// strncmp --> compara los LEN primeros bytes de CAD1 y CAD2
  	// 				/cad1/			  /cad2/ /len/		 
  	if(strncmp(&topic[topic_len - 4], "json", 4) != 0) {
  		// Si el formato NO es json, ignoramos
    	printf("Incorrect format\n");
  	}

  	if(strncmp(&topic[10], "leds", 4) == 0) {
    	if(chunk[0] == '1') {
      		leds_on(LEDS_RED);
    	}else if(chunk[0] == '0') {
      		leds_off(LEDS_RED);
    	}
    	return;
  }
}
// SOLO se llama a esta funcion cuando se REGISTRA la conexion MQTT
static void mqtt_event(struct mqtt_connection *m, mqtt_event_t event, void *data){
	// SOLO haces un case del TIPO DE EVENTO
  	switch(event) {
  		// Conectado al broker MQTT
  		case MQTT_EVENT_CONNECTED: {
    		DBG("APP - Application has a MQTT connection\n");
    		//							/Espera 5 seg, y si pasan, la conexión es ESTABLE/ 	
    		timer_set(&connection_life, CONNECTION_STABLE_TIME);
    		state = STATE_CONNECTED;
    		break;
  		}
  		// DESconectado del broker MQTT
  		case MQTT_EVENT_DISCONNECTED: {
    		DBG("APP - MQTT Disconnect. Reason %u\n", *((mqtt_event_t *)data));
	    	state = STATE_DISCONNECTED;
    		process_poll(&mqtt_demo_process);
    		break;
  		}
		case MQTT_EVENT_PUBLISH: {
    		mensaje_mqtt = data;
		    /* Implement first_flag in publish message? */
    		if(mensaje_mqtt->first_chunk) {
      			mensaje_mqtt->first_chunk = 0;
      			DBG("APP - Application received a publish on topic '%s'. Payload "
          		"size is %i bytes. Content:\n\n",
          		mensaje_mqtt->topic, mensaje_mqtt->payload_length);
    		}
		    pub_handler(mensaje_mqtt->topic, strlen(mensaje_mqtt->topic), mensaje_mqtt->payload_chunk,
                mensaje_mqtt->payload_length);
    		break;
  		}
  		// ACK de una subscripción a un tema
	  	case MQTT_EVENT_SUBACK: {
	    	DBG("APP - Application is subscribed to topic successfully\n");
	    	break;
	  	}
	  	// ACK de una des-subscripción a un tema
	  	case MQTT_EVENT_UNSUBACK: {
	    	DBG("APP - Application is unsubscribed to topic successfully\n");
	    	break;
	  	}
	  	// ACK de una publicación
	  	case MQTT_EVENT_PUBACK: {
	    	DBG("APP - Publishing complete\n");
	    	break;
	  	}
	  	default:
	    	DBG("APP - Application got a unhandled MQTT event: %i\n", event);
	    	break;
 	}
}

// Nos suscribimos a un tema en el broker
static void subscribe(void){

  	mqtt_status_t status;
  									//topic al que te subscribes/ 	/nivel de QoS/
  	status = mqtt_subscribe(&conn, NULL, 		sub_topic, 			MQTT_QOS_LEVEL_0);
  	// devuelve MQTT_STATUS_OK o algún error

  	DBG("APP - Subscribing!\n");
  	if(status == MQTT_STATUS_OUT_QUEUE_FULL) {
    	DBG("APP - Tried to subscribe but command queue was full!\n");
  	}
}

// Publicamos un tema en el broker
static void publish(void){
  	// Publicar el tema MQTT en el formato quickstart de IBM
  	int len;
  	int remaining = APP_BUFFER_SIZE; // 128
  	// publicaciones +1
  	seq_nr_value++;

  	buf_ptr = app_buffer;
  				  //destino/ 	/bytes a escribir/
  	len = snprintf(buf_ptr, 	  remaining,
                 "{"
                 "\"d\":{"
                 "\"Seq #\":%d,"
                 "\"Uptime (sec)\":%lu", seq_nr_value, clock_seconds());
  	//						/128/
  	if(len < 0 || len >= remaining) {
    	printf("Buffer too short. Have %d, need %d + \\0\n", remaining, len);
    	return;
  	}

  	remaining -= len;
  	buf_ptr += len;

  	/* Put our default route's string representation in a buffer */
  	char def_rt_str[64];
  	// ponemos todo a "0" en la variable
  	memset(def_rt_str, 0, sizeof(def_rt_str));
  	// funcion LOCAL
  	ipaddr_sprintf(def_rt_str, sizeof(def_rt_str), uip_ds6_defrt_choose());

  	len = snprintf(buf_ptr, remaining, ",\"Def Route\":\"%s\",\"RSSI (dBm)\":%d",
                 def_rt_str, def_rt_rssi);

  	if(len < 0 || len >= remaining) {
    	printf("Buffer too short. Have %d, need %d + \\0\n", remaining, len);
    	return;
  	}
  	remaining -= len;
  	buf_ptr += len;

  	len = snprintf(buf_ptr, remaining, ",\"On-Chip Temp (deg.C)\"");

  	if(len < 0 || len >= remaining) {
    	printf("Buffer too short. Have %d, need %d + \\0\n", remaining, len);
    	return;
  	}
  	remaining -= len;
  	buf_ptr += len;

  	len = snprintf(buf_ptr, remaining, "}}");

  	if(len < 0 || len >= remaining) {
    	printf("Buffer too short. Have %d, need %d + \\0\n", remaining, len);
    	return;
  	}
  	//						 /topic en el que publicas/	/puntero al payload del tema/
  	mqtt_publish(&conn, NULL,	 	pub_topic, 				(uint8_t *)app_buffer,
 			  //tamaño del payload/	 /nivel de QoS/
               strlen(app_buffer), 	MQTT_QOS_LEVEL_0, MQTT_RETAIN_OFF);

  	DBG("APP - Publish!\n");
}

static void connect_to_broker(void){
	// Connect to MQTT server								 (30 * 3) = 90 seg									 
	mqtt_connect(&conn, conf.broker_ip, conf.broker_port, conf.pub_interval * 3);
	state = STATE_CONNECTING;
}
// MANDAS UN PING A TU NODO PADRE RPL
static void ping_parent(void){
	// Si no tienes IP, no puedes mandar ping
  	if(uip_ds6_get_global(ADDR_PREFERRED) == NULL) {
    	return;
  	}			 //destino ("padre" RPL)/	  /tipo ICMP/      /codigo ICMP/  /longitud payload/
  	uip_icmp6_send(uip_ds6_defrt_choose(), ICMP6_ECHO_REQUEST,  	0, 		 ECHO_REQ_PAYLOAD_LEN);
}
// AQUI SOLO HACE UN CASE DEL state 
static void state_machine(void){

  	switch(state) {
	  	case STATE_INIT:
	  		// registra la conexion MQTT 						/funcion a llamar cuando responda el servidor/
	    	mqtt_register(&conn, &mqtt_demo_process, client_id, 			mqtt_event, 				MAX_TCP_SEGMENT_SIZE);
	    	/* Si no estamos utilizando el servicio de quickstart
	    	(por lo tanto, somos un dispositivo registrado de IBM),
			necesitamos proporcionar nombre de usuario y contraseña */
			// strncasecmp --> Igual qye strncmp, pero sin tener en cuenta MAYUSCULAS o MINUSCULAS
		    if(strncasecmp(conf.org_id, "quickstart", strlen(conf.org_id)) != 0) {
			    if(strlen(conf.auth_token) == 0) {
			        printf("User name set, but empty auth token\n");
			        state = STATE_ERROR;
			        break;
			    } else {							 //  /username/		 /password/
			        mqtt_set_username_password(&conn, "use-token-auth", conf.auth_token);
			    }
		    }
		    /* _register() will set auto_reconnect. We don't want that. */
		    conn.auto_reconnect = 0;
		    connect_attempt = 1;
		    state = STATE_REGISTERED;
		    DBG("Init\n");
		    // Continue..
	  	case STATE_REGISTERED:
	  		// mira si tiene IP asignada
	    	if(uip_ds6_get_global(ADDR_PREFERRED) != NULL) {
		      	DBG("Registered. Connect attempt %u\n", connect_attempt);
		      	// funcion local
		      	ping_parent();
		      	// otra funcion local
		      	connect_to_broker();
	    	}else {
	      		leds_on(LEDS_GREEN);
	      		// 	ctimer --> cuando acaba el tiempo, llama a una función. 
	      		//				Cada caso tiene una DURACION del led DISTINTA
	      		ctimer_set(&ct, NO_NET_LED_DURATION, publish_led_off, NULL);
	    	}
	    	etimer_set(&publish_periodic_timer, NET_CONNECT_PERIODIC);
	    	return;
	    	break;
	  	case STATE_CONNECTING:
	    	leds_on(LEDS_GREEN);
	    	ctimer_set(&ct, CONNECTING_LED_DURATION, publish_led_off, NULL);
	    	/* Not connected yet. Wait */
	    	DBG("Connecting (%u)\n", connect_attempt);
	    	break;
  		case STATE_CONNECTED:
    		/* No suscribirse a menos que seamos un dispositivo registrado */
    		if(strncasecmp(conf.org_id, "quickstart", strlen(conf.org_id)) == 0) {
     			DBG("Using 'quickstart': Skipping subscribe\n");
      			state = STATE_PUBLISHING;
    		}	
    		/* Continue */
  		case STATE_PUBLISHING:
    		/* Si el temporizador ha caducado, la conexión es estable. */
    		if(timer_expired(&connection_life)) {
      			/*
			    Intencionalmente usando 0 aquí en vez de 1: Queremos intentos
			    RECONNECT ATTEMPTS si nos desconectamos después de una 
			    conexión satisfactoria
			    */
		    	connect_attempt = 0;
    		}

		    if(mqtt_ready(&conn) && conn.out_buffer_sent) {
		      	if(state == STATE_CONNECTED) {
		        	// funcion local
		        	subscribe();
		        	state = STATE_PUBLISHING;
		      	} else {
		        	leds_on(LEDS_GREEN);
		        	ctimer_set(&ct, PUBLISH_LED_ON_DURATION, publish_led_off, NULL);
		        	// funcion local
		        	publish();
		      	}
		      	etimer_set(&publish_periodic_timer, conf.pub_interval);
				DBG("Publishing\n");
		      	/* Return here so we don't end up rescheduling the timer 
		      	   return aquí para que no terminemos la reprogramación del temporizador */
		      	return;
		    } else {
			    /*
			     Our publish timer fired, but some MQTT packet is already in flight
			     (either not sent at all, or sent but not fully ACKd).
			    
			     This can mean that we have lost connectivity to our broker or that
			     simply there is some network delay. In both cases, we refuse to
			     trigger a new message and we wait for TCP to either ACK the entire
			     packet after retries, or to timeout and notify us.
			    */
			    DBG("Publishing... (MQTT state=%d, q=%u)\n", conn.state, conn.out_queue_full);
		    }
		    break;
	  	case STATE_DISCONNECTED:
		    DBG("Disconnected\n");
		    // Si se cumple, VUELVE A PROBAR
		    if(connect_attempt < RECONNECT_ATTEMPTS ||	RECONNECT_ATTEMPTS == RETRY_FOREVER) {
		      	/* Disconnect and backoff */
		      	clock_time_t interval;
		      	mqtt_disconnect(&conn);
		      	connect_attempt++;				  // 2 segundos/
			    interval = connect_attempt < 3 ? RECONNECT_INTERVAL << connect_attempt : RECONNECT_INTERVAL << 3;

		      	DBG("Disconnected. Attempt %u in %lu ticks\n", connect_attempt, interval);
		      	etimer_set(&publish_periodic_timer, interval);

		      	state = STATE_REGISTERED;
		      	return;
		    }else{
		      	/* Max reconnect attempts reached. Enter error state */
		      	state = STATE_ERROR;
		      	DBG("Aborting connection after %u attempts\n", connect_attempt - 1);
		    }
		    break;
	  	case STATE_CONFIG_ERROR:
	    	/* Idle away. The only way out is a new config */
	    	printf("Bad configuration\n");
	    	return;
	  	case STATE_ERROR:
	  	default:
	    	leds_on(LEDS_GREEN);
		    /*
		     * 'default' should never happen.
		     * If we enter here it's because of some error. Stop timers. The only thing
		     * that can bring us out is a new config event
		     */
	    	printf("Default case: State=0x%02x\n", state);
	    	return;
	}
  	/* If we didn't return so far, reschedule ourselves */
  	etimer_set(&publish_periodic_timer, STATE_MACHINE_PERIODIC);
}

// AQUI CONSTRUYES el topic en el que DESPUES PUBLICARÁS
static int construct_pub_topic(void){
									// 64
  	int len = snprintf(pub_topic, BUFFER_SIZE, "iot-2/evt/%s/fmt/json",
                     conf.event_type_id);

  	if(len < 0 || len >= BUFFER_SIZE) {
    	printf("Pub Topic: %d, Buffer %d\n", len, BUFFER_SIZE);
    	return 0;
  	}
  	return 1;
}

// AQUI CONSTRUYES el topic al que DESPUES TE SUBSCRIBIRÁS
static int construct_sub_topic(void) {
									// 64
  	int len = snprintf(sub_topic, BUFFER_SIZE, "iot-2/cmd/%s/fmt/json",
                     conf.cmd_type);

    if(len < 0 || len >= BUFFER_SIZE) {
  		printf("Sub Topic: %d, Buffer %d\n", len, BUFFER_SIZE);
    	return 0;
  	}
	return 1;
}
// Construyes un ID para el cliente
static int construct_client_id(void){
									// 64
  	int len = snprintf(client_id, BUFFER_SIZE, "d:%s:%s:%02x%02x%02x%02x%02x%02x",
                     conf.org_id, conf.type_id,
                     linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
                     linkaddr_node_addr.u8[2], linkaddr_node_addr.u8[5],
                     linkaddr_node_addr.u8[6], linkaddr_node_addr.u8[7]);

  	if(len < 0 || len >= BUFFER_SIZE) {
    	printf("Client ID: %d, Buffer %d\n", len, BUFFER_SIZE);
    	return 0;
  	}
  	return 1;
}

static void update_config(void){
	// SI ESTAS FUNCIONES DAN "0" ES UN ERROR
  	if(construct_client_id() == 0) {
    	state = STATE_CONFIG_ERROR;
    	return;
  	}

  	if(construct_sub_topic() == 0) {
    	state = STATE_CONFIG_ERROR;
    	return;
  	}

  	if(construct_pub_topic() == 0) {
    	state = STATE_CONFIG_ERROR;
    	return;
  	}

  	// Reiniciar el contador
  	seq_nr_value = 0;
	state = STATE_INIT;

  	// Cuando acaba el TIMEOUT lanza un evento con:
  	// ev == PROCESS_EVENT_TIMER y data == &publish_periodic_timer
  	etimer_set(&publish_periodic_timer, 0);

  	return;
}

static int init_config(){
  	/* Configuración de los valores por defecto */
  	memset(&conf, 0, sizeof(mqtt_client_config_t));

  	memcpy(conf.org_id, DEFAULT_ORG_ID, strlen(DEFAULT_ORG_ID));
 	memcpy(conf.type_id, DEFAULT_TYPE_ID, strlen(DEFAULT_TYPE_ID));
  	memcpy(conf.auth_token, DEFAULT_AUTH_TOKEN, strlen(DEFAULT_AUTH_TOKEN));
  	memcpy(conf.event_type_id, DEFAULT_EVENT_TYPE_ID, strlen(DEFAULT_EVENT_TYPE_ID));
  	memcpy(conf.broker_ip, broker_ip, strlen(broker_ip));
  	memcpy(conf.cmd_type, DEFAULT_SUBSCRIBE_CMD_TYPE, 1);

  	conf.broker_port = DEFAULT_BROKER_PORT; 		// 1883
  	conf.pub_interval = DEFAULT_PUBLISH_INTERVAL;	// 30 seg
  	conf.def_rt_ping_interval = DEFAULT_RSSI_MEAS_INTERVAL; //30 seg

  	return 1;
}

PROCESS_THREAD(mqtt_demo_process, ev, data)
{

  	PROCESS_BEGIN();
  	printf("MQTT Demo Process\n");
  	// Si da error la conf. inicial
  	if(init_config() != 1) {
    	PROCESS_EXIT();
  	}
  	// otras confs. más
  	update_config();
  	// El RSSI con el router por defecto
  	def_rt_rssi = 0x80;
  	//		      											 /llama a esta función/						
  	uip_icmp6_echo_reply_callback_add(&echo_reply_notification, echo_reply_handler);  									
  	etimer_set(&echo_request_timer, conf.def_rt_ping_interval);

	(&button_sensor)->configure(SENSORS_ACTIVE, 1);

  	while(1) {
  		// Espera por un evento
    	PROCESS_YIELD();
    	// evento de sensor
    	if(ev == sensors_event && data == (&button_sensor)) {
      		if(state == STATE_ERROR) {
        		connect_attempt = 1;
        		state = STATE_REGISTERED;
      		}
    	}
    	// eventos de temporizador
    	if((ev == PROCESS_EVENT_TIMER && data == &publish_periodic_timer) ||
       		ev == PROCESS_EVENT_POLL || (ev == sensors_event && data == (&button_sensor) )) {
      		state_machine();
    	}
    	// Aqui solo se entra para mandar un ping a tu nodo "padre"
    	// cuando acaba el intervalo de "ping"
    	if(ev == PROCESS_EVENT_TIMER && data == &echo_request_timer) {
      		ping_parent();						// 30 segundos /	
      		etimer_set(&echo_request_timer, conf.def_rt_ping_interval);
    	}
  	}
  	PROCESS_END();
}
