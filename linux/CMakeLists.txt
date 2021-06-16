cmake_minimum_required(VERSION 3.0.0)
# set(CMAKE_C_COMPILER /usr/bin/x86_64-linux-gnu-gcc-9)
# set(CMAKE_CXX_COMPILER /usr/bin/x86_64-linux-gnu-gcc-9)
project(limero VERSION 0.1.0)
set(CMAKE_BUILD_TYPE Debug)

include(CTest)
enable_testing()

ADD_LIBRARY(limero limero.cpp MqttPaho.cpp MqttMosquitto.cpp Sys.cpp ../common/Base64.cpp ../common/Bytes.cpp Log.cpp Sema.cpp Gpio.cpp ) 

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
EXECUTE_PROCESS( COMMAND arch OUTPUT_VARIABLE ARCH OUTPUT_STRIP_TRAILING_WHITESPACE )
if ( ${ARCH} STREQUAL "armv6l" OR ${ARCH} STREQUAL "armv7l" )
	set(GCC_FLAGS "-DLINUX -DHAS_GPIO")
else()
	set(GCC_FLAGS "-DLINUX")
endif()
set(CMAKE_BUILD_TYPE Debug)
include(CPack)

add_definitions(${GCC_FLAGS})
include_directories( ../inc . 
../../ArduinoJson/src 
../../paho.mqtt.c/src 
../../mosquitto/include 
)
target_link_libraries(limero -lpthread 
-L/home/lieven/workspace/paho.mqtt.c/build/output/ -l:libpaho-mqtt3c.a 
-L/home/lieven/workspace/Common/Debug/ 
-lCommon -
lrt 
-pthread )
