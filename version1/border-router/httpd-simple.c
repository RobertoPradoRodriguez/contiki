#include <stdio.h>
#include <string.h>

#include "contiki-net.h"

#include "httpd-simple.h"
#define webserver_log_file(...)
#define webserver_log(...)

#ifndef WEBSERVER_CONF_CFS_CONNS
#define CONNS UIP_CONNS
#else /* WEBSERVER_CONF_CFS_CONNS */
#define CONNS WEBSERVER_CONF_CFS_CONNS
#endif /* WEBSERVER_CONF_CFS_CONNS */

#ifndef WEBSERVER_CONF_CFS_URLCONV
#define URLCONV 0
#else /* WEBSERVER_CONF_CFS_URLCONV */
#define URLCONV WEBSERVER_CONF_CFS_URLCONV
#endif /* WEBSERVER_CONF_CFS_URLCONV */

#define STATE_WAITING 0
#define STATE_OUTPUT  1

// AQUI DECLARA UN BLOQUE DE MEMORIA
	//nombre/	/estructura/  /numero de fragmentos del bloque/
MEMB(conns, struct httpd_state, CONNS);

#define ISO_nl      0x0a
#define ISO_space   0x20
#define ISO_period  0x2e
#define ISO_slash   0x2f


static const char *NOT_FOUND = 	"<html><body bgcolor=\"white\">"
							   	               "<center>"
									               "<h1>404 - file not found</h1>"
								                 "</center>"
								                "</body></html>";

/////////////////////// PROTO SOCKET /////////////////////////////
static PT_THREAD(send_string(struct httpd_state *s, const char *str))
{
  	PSOCK_BEGIN(&s->sout);
  	SEND_STRING(&s->sout, str);
  	PSOCK_END(&s->sout);
}

const char http_content_type_html[] = "Content-type: text/html\r\n\r\n";
/////////////////////////////////////////////////////////////////////
/////////////////////// PROTO SOCKET ////////////////////////////////
static PT_THREAD(send_headers(struct httpd_state *s, const char *statushdr))
{
	PSOCK_BEGIN(&s->sout);
	SEND_STRING(&s->sout, statushdr);
  	SEND_STRING(&s->sout, http_content_type_html);
  	PSOCK_END(&s->sout);
}

const char http_header_200[] = "HTTP/1.0 200 OK\r\nServer: Contiki/2.4 http://www.sics.se/contiki/\r\nConnection: close\r\n";
const char http_header_404[] = "HTTP/1.0 404 Not found\r\nServer: Contiki/2.4 http://www.sics.se/contiki/\r\nConnection: close\r\n";
///////////////////////////////////////////////////////////////////
/////////////////////// PROTO SOCKET //////////////////////////////
static PT_THREAD(handle_output(struct httpd_state *s))
{
  	// DESDE ESTE PROTOSOCKET LLAMA A OTROS DOS PROTOSOCKETS
  	PT_BEGIN(&s->outputpt);
  	s->script = NULL;
  	s->script = httpd_simple_get_script(&s->filename[1]);
  	if(s->script == NULL) {
    	strncpy(s->filename, "/notfound.html", sizeof(s->filename));
    	PT_WAIT_THREAD(&s->outputpt,send_headers(s, http_header_404));
    	PT_WAIT_THREAD(&s->outputpt,send_string(s, NOT_FOUND));
    	uip_close();
    	webserver_log_file(&uip_conn->ripaddr, "404 - not found");
    	PT_EXIT(&s->outputpt);
  	}else {
    	PT_WAIT_THREAD(&s->outputpt,send_headers(s, http_header_200));
    	PT_WAIT_THREAD(&s->outputpt, s->script(s));
  	}
  	s->script = NULL;
  	PSOCK_CLOSE(&s->sout);
  	PT_END(&s->outputpt);
}

const char http_get[] = "GET ";
const char http_index_html[] = "/index.html";
///////////////////////////////////////////////////////////////
///////////////////// PROTO SOCKET ////////////////////////////
static PT_THREAD(handle_input(struct httpd_state *s))
{
  	PSOCK_BEGIN(&s->sin);
  	PSOCK_READTO(&s->sin, ISO_space);

  	if(strncmp(s->inputbuf, http_get, 4) != 0) {
    	PSOCK_CLOSE_EXIT(&s->sin);
  	}
  	PSOCK_READTO(&s->sin, ISO_space);

  	if(s->inputbuf[0] != ISO_slash) {
    	PSOCK_CLOSE_EXIT(&s->sin);
  	}

#if URLCONV
  	s->inputbuf[PSOCK_DATALEN(&s->sin) - 1] = 0;
  	urlconv_tofilename(s->filename, s->inputbuf, sizeof(s->filename));
#else /* URLCONV */
  	if(s->inputbuf[1] == ISO_space) {
    	strncpy(s->filename, http_index_html, sizeof(s->filename));
  	}else {
    	s->inputbuf[PSOCK_DATALEN(&s->sin) - 1] = 0;
    	strncpy(s->filename, s->inputbuf, sizeof(s->filename));
  	}
#endif /* URLCONV */
	webserver_log_file(&uip_conn->ripaddr, s->filename);
	s->state = STATE_OUTPUT;

  	while(1) {
    	PSOCK_READTO(&s->sin, ISO_nl);
#if 0
    	if(strncmp(s->inputbuf, http_referer, 8) == 0) {
      		s->inputbuf[PSOCK_DATALEN(&s->sin) - 2] = 0;
      		webserver_log(s->inputbuf);
    	}
#endif
  	}
  	PSOCK_END(&s->sin);
}

static void handle_connection(struct httpd_state *s)
{
  	// LLAMA A DOS PROTOSOCKETS DE LOS CUATRO
  	handle_input(s);
  	if(s->state == STATE_OUTPUT){
    	handle_output(s);
  	}
}

void httpd_appcall(void *state)
{
  	struct httpd_state *s = (struct httpd_state *)state;
  	//conexion CERRADA o ABORTADA por el otro extremo, o CADUCADA
  	if(uip_closed() || uip_aborted() || uip_timedout()) {
		if(s != NULL) {
      		s->script = NULL;
      		// LIBERA BLOQUE DE MEMORIA
      		//nombre /puntero al bloque/
      		memb_free(&conns, s);
    	}
    // ¿HAY CONEXION?
  	}else if(uip_connected()) {
    	s = (struct httpd_state *)memb_alloc(&conns);
    	// SI NO HAY MEMORIA RESERVADA
    	if(s == NULL) {
      		uip_abort();
      		webserver_log_file(&uip_conn->ripaddr, "reset (no memory block)");
      		return;
    	}
    	tcp_markconn(uip_conn, s);
    	// Inicializa un PROTOSOCKET
    	PSOCK_INIT(&s->sin, (uint8_t *)s->inputbuf, sizeof(s->inputbuf) - 1);
    	PSOCK_INIT(&s->sout, (uint8_t *)s->inputbuf, sizeof(s->inputbuf) - 1);
    	// INICIALIZA UN PROTOTHREAD
    	PT_INIT(&s->outputpt);
    	s->script = NULL;
    	s->state = STATE_WAITING;
    	timer_set(&s->timer, CLOCK_SECOND * 10);
    	handle_connection(s);
  	}else if(s != NULL) {
    	if(uip_poll()) {
      		if(timer_expired(&s->timer)) {
        		uip_abort();
        		s->script = NULL;
        		memb_free(&conns, s);
        		webserver_log_file(&uip_conn->ripaddr, "reset (timeout)");
    		}
    	}else{
    		timer_restart(&s->timer);
    	}
    	handle_connection(s);
  	}
  	else {
    	uip_abort();
  	}
}

void httpd_init(void)
{
	// ABRE UN PUERTO TCP
  	tcp_listen(UIP_HTONS(80));
  	// INICIALIZA EL BLOQUE DE MEMORIA DECLARADO ANTERIORMENTE
  	memb_init(&conns);
	#if URLCONV
  		urlconv_init();
	#endif
}