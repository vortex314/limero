
#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := nanoEsp8266
IDF_PATH := $(HOME)/esp/ESP8266_RTOS_SDK

TTY ?= USB0
SERIAL_PORT ?= /dev/tty$(TTY)
ESPPORT = $(SERIAL_PORT)
SERIAL_BAUD = 115200
ESPBAUD=921600

CPPFLAGS= -DEXTRAS_MDNS_RESPONDER -DLWIP_MDNS_RESPONDER=1 -DLWIP_NUM_NETIF_CLIENT_DATA=1 -DLWIP_NETIF_EXT_STATUS_CALLBACK=1  
CPPFLAGS+= -DWIFI_SSID=$(SSID) -DWIFI_PASS=$(PSWD)
CPPFLAGS+= -DMQTT_HOST=limero.ddns.net -DMQTT_PORT=1883 -DESP8266_RTOS_SDK
CPPFLAGS+= -DTREESHAKER

CXXFLAGS += -mlongcalls
CFLAGS += -mlongcalls

COMPONENT_ADD_INCLUDEDIRS=../../main ../../../Common ../../Common 
EXTRA_COMPONENT_DIRS=../Common
WORKSPACE = $(HOME)/workspace

include $(IDF_PATH)/make/project.mk

term:
	rm -f $(TTY)_minicom.log
	minicom -D $(SERIAL_PORT) -b $(SERIAL_BAUD) -C $(TTY)_minicom.log