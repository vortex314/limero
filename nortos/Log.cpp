
#include "Log.h"

#include <assert.h>
#include <string.h>

#include <chrono>
#include <ctime>
#include <iomanip>
#include <printf.h>

extern "C" void uartSendBytes(uint8_t* ,size_t,uint32_t);

#define LOG_SIZE  256

Log::Log(){
	_logWriter=0;
	_level=L_INFO;
	_buffer=(char*)(new char[LOG_SIZE]);
	_bufferSize=LOG_SIZE;
	offset=0;
	txBufferOverflow=0;
	txBusy=false;
}


Log& Log::logf(const char *format, ...) {
	if ( txBusy ) return *this;
	if (offset > _bufferSize) return *this;
	va_list args;
	va_start(args, format);
	offset +=
			vsnprintf(&Log::_buffer[offset], _bufferSize - offset, format, args);
	va_end(args);
	return *this;
}

void Log::flush() {
	if ( txBusy ) return;
	if (offset >= (_bufferSize - 2)) return;
	Log::_buffer[offset++] = '\r';
	Log::_buffer[offset++] = '\n';
	txBusy=true;
	uartSendBytes((uint8_t*)Log::_buffer,offset,0);
	offset = 0;
	txBusy=false;
}

Log& Log::tfl(const char * lvl,const char *file, const uint32_t line) {
	if ( txBusy ) return *this;
	uint64_t t = Sys::millis();
	uint32_t sec = t / 1000;
	uint32_t msec = t % 1000;
	uint32_t min = sec / 60  ;
	uint32_t hour = min / 60;

	offset = snprintf(_buffer, _bufferSize, 
	"%s %3.3u:%2.2u:%2.2u.%3.3u | %s:%4lu | ",
	lvl, hour ,min %60,sec % 60,msec, file, line);
	return *this;
}