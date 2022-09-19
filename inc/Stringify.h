#ifndef __STRINGIFY_H__
#define __STRINGIFY_H__
#include <StringUtility.h>
#include <printf.h>

#include <string>

#define TOSTRING(x) STRINGIFY(x)
#define AT __FILE__ ":" TOSTRING(__LINE__)

#define TYPE_TO_STRING(TT) \
  class TT;                \
  inline const char* typeToString(const TT*) { return #TT; }
#define BASE_TO_STRING(TT) \
  inline const char* typeToString(const TT*) { return #TT; }

TYPE_TO_STRING(Json)
TYPE_TO_STRING(TimerMsg)
TYPE_TO_STRING(UdpMsg)
BASE_TO_STRING(std::vector<uint8_t>)
BASE_TO_STRING(bool)
BASE_TO_STRING(int)
BASE_TO_STRING(unsigned int)
BASE_TO_STRING(long)
BASE_TO_STRING(float)
BASE_TO_STRING(double)
BASE_TO_STRING(std::string)
BASE_TO_STRING( char*)


inline std::string toString(bool b) { return b ? "true" : "false"; }

inline std::string toString(const float f) {
  char buffer[32];
  sprintf_(buffer, "%f", f);
  return buffer;
}
inline std::string toString(const int& t) {
  char buffer[32];
  sprintf_(buffer, "%d", t);
  return buffer;
}
inline std::string toString(const unsigned int t) {
  char buffer[32];
  sprintf_(buffer, "%u", t);
  return buffer;
}
inline std::string toString(const unsigned int& t) {
  char buffer[32];
  sprintf_(buffer, "%u", t);
  return buffer;
}
inline std::string toString(const unsigned long long& t) {
#ifdef ESP8266_RTOS_SDK
  std::string s = stringFormat("%llu", t);
  return s;
#else
  char buffer[32];
  sprintf_(buffer, "%llu", t);
  return buffer;
#endif
}

inline std::string toString(const unsigned long& t) {
  char buffer[32];
  sprintf_(buffer, "%lu", t);
  return buffer;
}
inline std::string toString(const long& t) {
  char buffer[32];
  sprintf_(buffer, "%ld", t);
  return buffer;
}
inline std::string toString(const char* t) { return std::string(t); }
inline std::string toString(const std::string& t) {
  std::string s = "\"" + t + "\"";
  return s;
}
inline std::string toString(const char* t, size_t len) {
  return std::string(t, len);
}

#endif