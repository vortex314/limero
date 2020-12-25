cmake_minimum_required(VERSION 3.0.0)
project(limero VERSION 0.1.0)

include(CTest)
enable_testing()

ADD_LIBRARY(limero limero.cpp MqttPaho.cpp Sys.cpp ../common/Base64.cpp ../common/Bytes.cpp Log.cpp Sema.cpp Gpio.cpp ) 

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
set(GCC_FLAGS "-DX86_LINUX")
set(CMAKE_BUILD_TYPE Debug)
include(CPack)

add_definitions(${GCC_FLAGS})
include_directories( ../inc . ../../ArduinoJson/src ../../paho.mqtt.c/src  )
target_link_libraries(limero -lpthread -L/home/lieven/workspace/paho.mqtt.c/build/output/ -l:libpaho-mqtt3c.a -L/home/lieven/workspace/Common/Debug/ -lCommon -lrt -pthread )
