set(MQTT_HOST limero.ddns.net)
set(MQTT_PORT 1883)

idf_component_register(
    SRCS limero.cpp Wifi.cpp Sys.cpp Sema.cpp  Log.cpp Hardware.cpp ../src/StringUtility.cpp ../src/printf.c Client.cpp Serial.cpp
    INCLUDE_DIRS . ../inc ../../ArduinoJson/src 
    REQUIRES freertos nvs_flash mqtt esp_https_ota app_update esp_adc esp_hw_support lwip esp_rom esp_timer driver esp_wifi )