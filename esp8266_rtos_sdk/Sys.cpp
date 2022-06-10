#include <Log.h>
#include <Sys.h>
#include <string.h>
#include <unistd.h>

uint64_t Sys::_upTime;
char Sys::_hostname[30] = "";
uint64_t Sys::_boot_time = 0;

#include <esp_system.h>
#include <esp_timer.h>
#include <Log.h>
#include <Sys.h>
#include <stdint.h>
#include <sys/time.h>

const char *Sys::getProcessor()
{
    return "ESP8266";
}
const char *Sys::getBuild() { return __DATE__ " " __TIME__; }

void Sys::init()
{
    // uint8_t cpuMhz = sdk_system_get_cpu_frequency();
}
uint32_t Sys::getFreeHeap() { return esp_get_free_heap_size(); };

uint64_t Sys::millis()
{
    return esp_timer_get_time() / 1000;
}

extern "C" void vPortEnterCritical(void);
extern "C" void vPortExitCritical();

uint64_t Sys::micros()
{
    return esp_timer_get_time();
    /*
        static uint32_t lsClock = 0;
        static uint32_t msClock = 0;

        vPortEnterCritical();
        uint32_t ccount;
        __asm__ __volatile__("esync; rsr %0,ccount"
                             : "=a"(ccount));
        if (ccount < lsClock)
        {
            msClock++;
        }
        lsClock = ccount;
        vPortExitCritical();

        uint64_t micro = msClock;
        micro <<= 32;
        micro += lsClock;

        return micro / 80;*/
}

uint64_t Sys::now() { return _boot_time + Sys::millis(); }

void Sys::setNow(uint64_t n) { _boot_time = n - Sys::millis(); }

void Sys::hostname(const char *h) { strncpy(_hostname, h, strlen(h) + 1); }

void Sys::setHostname(const char *h) { strncpy(_hostname, h, strlen(h) + 1); }

void Sys::delay(unsigned int delta)
{
    uint32_t end = Sys::millis() + delta;
    while (Sys::millis() < end)
        ;
}

uint32_t Sys::sec() { return millis() / 1000; }

extern "C" uint64_t SysMillis() { return Sys::millis(); }

const char *Sys::hostname()
{

    if (_hostname[0] == 0)
    {

        uint8_t mac[6];
        esp_efuse_mac_get_default(mac);
        uint32_t l = mac[2] * mac[3] * mac[4] * mac[5];
        snprintf(_hostname, sizeof(_hostname), "ESP82-%u", l);
    }
    return _hostname;
}
