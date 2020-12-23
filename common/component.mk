# Component makefile for extras/rboot-ota

INC_DIRS += $(common_ROOT)

# args for passing into compile rule generation
common_SRC_DIR =  $(common_ROOT)
common_CFLAGS = -DESP_OPEN_RTOS

common_CXXFLAGS += -g -ffunction-sections -fdata-sections -fno-threadsafe-statics 
common_CXXFLAGS += -std=c++11 -fno-rtti -lstdc++ -fno-exceptions 
common_CXXFLAGS += -DWIFI_PASS=${PSWD} -DWIFI_SSID=${SSID} $(DEFINEK) -DESP_OPEN_RTOS 

$(eval $(call component_compile_rules,common))
