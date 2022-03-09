#ifndef _STRING_UTILITY_
#define _STRING_UTILITY_
#include <string>
#include <vector>
std::string stringFormat(const char *fmt, ...);
std::string hexDump(const std::vector<uint8_t> & v,const char spacer=' ');
std::string charDump(const std::vector<uint8_t> &);
std::vector<std::string> split(const std::string &s, char seperator);

#endif