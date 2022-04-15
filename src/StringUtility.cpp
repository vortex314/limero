#include <StringUtility.h>
#include <stdarg.h>
#include <string.h>
#include <Log.h>
#include <printf.h>
#include <string>

std::string hexDump(const std::vector<uint8_t> &bs, const char spacer)
{
  static char HEX_DIGITS[] = "0123456789ABCDEF";
  std::string out;
  for (uint8_t b : bs)
  {
    out += HEX_DIGITS[b >> 4];
    out += HEX_DIGITS[b & 0xF];
    out += spacer;
  }
  return out;
}

std::string charDump(const std::vector<uint8_t> &bs)
{
  std::string out;
  for (uint8_t b : bs)
  {
    if (isprint(b))
      out += (char)b;
    else if (b == '\n')
      out += "\n";
    else if (b == '\r')
      out += "\r";
    else if (b == '\t')
      out += "\t";
    else
      out += ".";
  }
  return out;
}

std::string stringFormat(const char *fmt, ...)
{
  static std::string str;
  str.clear();
  int size = strlen(fmt) * 2 + 50; // Use a rubric appropriate for your code
  if (size > 10240)
    INFO(" invalid log size\n");
  va_list ap;
  while (1)
  { // Maximum two passes on a POSIX system...
    if (size > 10240)
      break;
    str.resize(size);
    va_start(ap, fmt);
    int n = vsnprintf((char *)str.data(), str.capacity(), fmt, ap);
    va_end(ap);
    if (n > -1 && n < size)
    { // Everything worked
      str.resize(n);
      return str.c_str();
    }
    if (n > -1)     // Needed size returned
      size = n + 1; // For null char
    else
      size *= 2; // Guess at a larger size (OS specific)
  }
  return str;
}

std::vector<std::string> split(const std::string &s, char seperator)
{
  std::vector<std::string> output;
  std::string::size_type prev_pos = 0, pos = 0;
  while ((pos = s.find(seperator, pos)) != std::string::npos)
  {
    std::string substring(s.substr(prev_pos, pos - prev_pos));
    output.push_back(substring);
    prev_pos = ++pos;
  }
  output.push_back(s.substr(prev_pos, pos - prev_pos)); // Last word
  return output;
}
