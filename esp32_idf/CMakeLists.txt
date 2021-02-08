idf_component_register(
    SRCS limero.cpp Wifi.cpp Sys.cpp Sema.cpp MqttWifi.cpp MqttSerial.cpp MqttOta.cpp Log.cpp Hardware.cpp 
    INCLUDE_DIRS . ../inc ../../ArduinoJson/src 
    REQUIRES freertos nvs_flash mqtt esp_https_ota app_update esp_adc_cal )