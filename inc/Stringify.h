#include <string>
inline std::string toString(const float f){
    return std::to_string(f);
}
inline std::string toString(const int &t)
{
    return std::to_string(t);
}
inline std::string toString(const unsigned long long &t)
{
    return std::to_string(t);
}

inline std::string toString(const unsigned long &t)
{
    return std::to_string(t);
}
inline std::string toString(const  long &t)
{
    return std::to_string(t);
}
inline std::string toString(const char *t)
{
    return std::string(t);
}
inline std::string toString(const std::string &t)
{
    return t;
}
inline std::string toString(const char *t, size_t len)
{
    return std::string(t, len);
}
