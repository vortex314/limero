
#include "Log.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <chrono>
#include <ctime>
#include <iomanip>

void consoleLogWriter(char *line, size_t length) {
  fwrite(line, length, 1, stdout);
  fwrite("\n", 1, 1, stdout);
}

Log::Log() {
  _bufferSize = 10240;
  _buffer = new char[_bufferSize];
  _logWriter = consoleLogWriter;
}

Log &Log::logf(const char *format, ...) {
  if (txBusy) return *this;
  if (offset > _bufferSize) return *this;
  va_list args;
  va_start(args, format);
  offset +=
      vsnprintf(&Log::_buffer[offset], _bufferSize - offset, format, args);
  va_end(args);
  return *this;
}

void Log::setLevel(Level level) { _level = level; }

void Log::flush() {
  if (txBusy) return;
  if (_logWriter)
    _logWriter(_buffer, offset);
  else
    consoleLogWriter(_buffer, offset);
  offset = 0;
  txBusy = false;
}
#include <sys/time.h>
#define MSEC_PER_HOUR 3600 * 1000
#define MSEC_PER_MIN 60 * 1000

Log &Log::tfl(const char *lvl, const char *file, const uint32_t line) {
  if (txBusy) return *this;
  struct timespec ts = {0, 0};

  char timbuf[64];
  if (clock_gettime(CLOCK_REALTIME, &ts)) {
    perror("clock_gettime"), exit(EXIT_FAILURE);
  };
  time_t tim = ts.tv_sec;
  struct tm *tm = localtime(&tim);
  strftime(timbuf, sizeof(timbuf), "%H:%M:%S", tm);
  uint32_t msec = ts.tv_nsec / 1000000;

  offset = snprintf(_buffer, _bufferSize, "%s %s.%3.3u | %15.15s:%4u | ", lvl,
                    timbuf, msec, file, line);
  return *this;
}

extern "C" void _putchar(char c) { putchar(c); }
