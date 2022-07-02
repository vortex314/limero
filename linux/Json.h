#ifndef __JSON_H__
#define __JSON_H__
#include <ArduinoJson.h>

class Json : public DynamicJsonDocument {
 public:
  Json() : DynamicJsonDocument(1024000) {}
  Json(int x) : DynamicJsonDocument(x){};
  Json(DynamicJsonDocument jsd) : DynamicJsonDocument(jsd) {}
  std::string toString() {
    std::string str;
    serializeJson(*this, str);
    return str;
  }
};
#endif