#ifndef __STRINGIFY_H__
#define __STRINGIFY_H__
#include <string>
#include <printf.h>
#include <StringUtility.h>

#define TOSTRING(x) STRINGIFY(x)
#define AT __FILE__ ":" TOSTRING(__LINE__)
#define TYPE_TO_STRING(x) #x
class TimerMsg;
inline const char* typeToString(const TimerMsg*) {
    return "TimerMsg";
}
inline const char* typeToString(const std::vector<uint8_t>*) {
    return "Bytes";
}
inline const char* typeToString(const bool*) {
    return "bool";
}

inline const char* typeToString(const  int*) {
    return "int";
}

inline const char* typeToString(const  unsigned int*) {
    return "unsigned int";
}

inline const char* typeToString(const long*) {
    return "long";
}


inline const char* typeToString(const unsigned long*) {
    return "unsigned long";
}

inline const char* typeToString(const long long*) {
    return "long long";
}

inline const char* typeToString(const unsigned long long*) {
    return "unsigned long long";
}

inline const char* typeToString(const float) {
    return "float";
}

inline const char* typeToString(const double*) {
    return "double";
}

inline const char* typeToString(const std::string*) {
    return "std::string";
}

inline const char* typeToString(const char**) {
    return "const char*";
}

inline const char* typeToString(char**) {
    return "char*";
}

inline const char* typeToString(char*) {
    return "char";
}

inline const char* typeToString(unsigned char*) {
    return "unsigned char";
}

inline const char* typeToString(void**) {
    return "void*";
}



inline std::string toString(bool b)
{
    return b?"true":"false";
}

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
inline std::string toString(const unsigned int t)
{
    char buffer[32];
    sprintf_(buffer, "%u", t);
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

#endif