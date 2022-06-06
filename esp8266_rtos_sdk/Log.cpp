/*
 * Log.cpp
 *
 *  Created on: Jul 3, 2016
 *      Author: lieven
 */
#include <Log.h>
extern "C"
{
#include <stdarg.h>
#include <printf.h>
#include <stdlib.h>
}
Log::Log()
{
  _bufferSize = 512;
  _buffer = new char[_bufferSize];
}

Log &Log::logf(const char *format, ...)
{
  if (txBusy)
    return *this;
  if (offset > _bufferSize)
    return *this;
  va_list args;
  va_start(args, format);
  offset += vsnprintf(&Log::_buffer[offset], _bufferSize - offset,
                      format, args);
  va_end(args);
  return *this;
}

void Log::setLevel(Level level)
{
  _level = level;
}

void Log::flush()
{
  if (txBusy)
    return;
  printf("%s\n", _buffer);
  offset = 0;
  txBusy = false;
}
#include <sys/time.h>
Log &Log::tfl(const char *lvl, const char *file, const uint32_t line)
{
  if (txBusy)
    return *this;
  uint64_t msec = Sys::millis();
  uint32_t sec = msec / 1000;
  uint32_t min = sec / 60;
  uint32_t hr = min / 60;
  offset = snprintf(_buffer, _bufferSize,
                    "%s %2.2u:%2.2d:%2.2d.%3.3lld | %15.15s:%4u | ", lvl,
                    hr % 24,
                    min % 60,
                    sec % 60,
                    msec % 1000,
                    file, line);
  return *this;
}

extern "C" void _putchar(char c)
{
  putchar(c);
}
