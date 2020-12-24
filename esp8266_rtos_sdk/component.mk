#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)

COMPONENT_ADD_INCLUDEDIRS= . ../components/config #relative
COMPONENT_PRIV_INCLUDEDIRS=  ../include  ../../../ArduinoJson/src  ../../ArduinoJson/src ../ArduinoJson/src
