# Component makefile for extras/rboot-ota

INC_DIRS += $(esp_open_rtos_ROOT)

# args for passing into compile rule generation
esp_open_rtos_SRC_DIR =  $(esp_open_rtos_ROOT)
esp_open_rtos_CFLAGS = -DESP_OPEN_RTOS

esp_open_rtos_CXXFLAGS += -g -ffunction-sections -fdata-sections -fno-threadsafe-statics 
esp_open_rtos_CXXFLAGS += -std=c++11 -fno-rtti -lstdc++ -fno-exceptions 
esp_open_rtos_CXXFLAGS += -DWIFI_PASS=${PSWD} -DWIFI_SSID=${SSID} $(DEFINEK) -DESP_OPEN_RTOS 

$(eval $(call component_compile_rules,esp_open_rtos))