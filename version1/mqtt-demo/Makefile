DEFINES+=PROJECT_CONF_H=\"project-conf.h\"

all: mqtt-demo

CONTIKI_WITH_IPV6 = 1
CONTIKI_WITH_RPL = 0

CFLAGS += -DUIP_CONF_RPL_IPV6=1

APPS += mqtt

CONTIKI=../../contiki-3.0
include $(CONTIKI)/Makefile.include
