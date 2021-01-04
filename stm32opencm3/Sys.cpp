#include <Log.h>
#include <Sys.h>
#include <string.h>
#include <unistd.h>

uint64_t Sys::_upTime;
char Sys::_hostname[30] = "";
uint64_t Sys::_boot_time = 0;

#include <FreeRTOS.h>
#include <task.h>
#include <stdint.h>
#include <sys/time.h>

const char *Sys::getProcessor()
{
    return "STM32";
}
const char *Sys::getBuild() { return __DATE__ " " __TIME__; }

uint64_t Sys::millis()
{
    static uint32_t msw=0;
    static uint32_t lsw=0;
    uint32_t newLsw = xTaskGetTickCount();
    if ( newLsw < lsw ) msw++;
    uint64_t t =  msw;
    t <<= 32;
    t+=newLsw;
    return t;
}

uint32_t Sys::sec() { return millis() / 1000; }

uint64_t Sys::now() { return _boot_time + Sys::millis(); }

void Sys::setNow(uint64_t n) { _boot_time = n - Sys::millis(); }

void Sys::hostname(const char *h) { strncpy(_hostname, h, strlen(h) + 1); }

void Sys::setHostname(const char *h) { strncpy(_hostname, h, strlen(h) + 1); }

void Sys::delay(uint32_t delta)
{
    vTaskDelay(delta);
}

extern "C" uint64_t SysMillis() { return Sys::millis(); }

const char *Sys::hostname()
{

    if (_hostname[0] == 0)
    {
        snprintf(_hostname, sizeof(_hostname), "%s", "stm32");
    }
    return _hostname;
}
const char *Sys::board()
{
    return "guess ? ";
}
