set(MQTT_HOST limero.ddns.net)
set(MQTT_PORT 1883)

idf_component_register(
    SRCS limero.cpp Wifi.cpp Sys.cpp Sema.cpp  Log.cpp Hardware.cpp ../src/StringUtility.cpp ../src/printf.c
    INCLUDE_DIRS . ../inc ../../ArduinoJson/src 
    REQUIRES freertos nvs_flash mqtt esp_https_ota app_update esp_adc_cal esp_hw_support lwip esp_rom )