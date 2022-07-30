#ifndef __CONFIG_FILE_H__
#include <ArduinoJson.h>
#include <Json.h>
#include <Log.h>
#include <unistd.h>

#include <string>
std::string loadFile(const char* name);
int configurator(Json& config, int argc, char** argv,
                 const char* configFile = "config.json");
void deepMerge(JsonVariant dst, JsonVariant src) ;
#endif