#include <string>
#include <printf.h>
#include <StringUtility.h>

inline std::string toString(const float f){
    char buffer[32];
    sprintf_(buffer, "%f", f);
    return buffer;
}
inline std::string toString(const int &t)
{
    char buffer[32];
    sprintf_(buffer, "%d", t);
    return buffer;
}
inline std::string toString(const unsigned int &t)
{
    char buffer[32];
    sprintf_(buffer, "%u", t);
    return buffer;
}
inline std::string toString(const unsigned long long &t)
{
    #ifdef ESP8266_RTOS_SDK
    std::string s = stringFormat("%llu", t);
    return s;
    #else
    char buffer[32];
    sprintf_(buffer, "%llu", t);
    return buffer;
    #endif
    
}


inline std::string toString(const unsigned long &t)
{
    char buffer[32];
    sprintf_(buffer, "%lu", t);
    return buffer;
}
inline std::string toString(const  long &t)
{
    char buffer[32];
    sprintf_(buffer, "%ld", t);
    return buffer;
}
inline std::string toString(const char *t)
{
    return std::string(t);
}
inline std::string toString(const std::string &t)
{
    std::string s ="\"" + t + "\"";
    return s;
}
inline std::string toString(const char *t, size_t len)
{
    return std::string(t, len);
}
