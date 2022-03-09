
#include "Log.h"

#include <assert.h>
#include <string.h>

#include <chrono>
#include <ctime>
#include <iomanip>
#include <printf.h>

extern "C" void uartSendBytes(uint8_t* ,size_t,uint32_t);


Log::Log(){

}


Log& Log::logf(const char *format, ...) {
	if ( txBusy ) return *this;
	if (offset > sizeof(_buffer)) return *this;
	va_list args;
	va_start(args, format);
	offset +=
			vsnprintf(&Log::_buffer[offset], sizeof(Log::_buffer) - offset, format, args);
	va_end(args);
	return *this;
}

void Log::flush() {
	if ( txBusy ) return;
	if (offset >= (sizeof(_buffer) - 2)) return;
	Log::_buffer[offset++] = '\r';
	Log::_buffer[offset++] = '\n';
	txBusy=true;
	uartSendBytes((uint8_t*)Log::_buffer,offset,0);
	offset = 0;
	txBusy=false;
}

Log& Log::tfl(const char *file, const uint32_t line) {
	if ( txBusy ) return *this;
	uint32_t t = Sys::millis();
	offset = snprintf(_buffer, sizeof(_buffer), "%8.8lu | %s:%4lu | ",  t, file, line);
	return *this;
}