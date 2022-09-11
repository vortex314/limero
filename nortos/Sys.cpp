#include "Sys.h"

#include <string>



#if PIOFRAMWORK == libopenCM3
#include <libopencm3/lm4f/rcc.h>
static volatile uint64_t system_millis;

/* Called when systick fires */
void sys_tick_handler(void)
{
	system_millis++;
}

/* simple sleep for delay milliseconds */
uint64_t Sys::millis()
{
	return system_millis;
}
enum
{
	PLL_DIV_80MHZ = 5,
	PLL_DIV_57MHZ = 7,
	PLL_DIV_40MHZ = 10,
	PLL_DIV_20MHZ = 20,
	PLL_DIV_16MHZ = 25,
};
void Sys::init()
{
	rcc_sysclk_config(OSCSRC_MOSC, XTAL_16M, PLL_DIV_80MHZ);
}
#endif


#ifdef __STM32__
#include "stm32f1xx_hal.h"

uint64_t __millis = 0;

uint64_t Sys::millis()
{
	static uint32_t major = 0;
	static uint32_t prevTick = 0;
	uint32_t newTick = HAL_GetTick();
	if (newTick < prevTick)
	{
		major++;
	}
	prevTick = newTick;
	__millis = major;
	__millis <<= 32;
	__millis += newTick;
	return __millis;
}

static inline uint32_t LL_SYSTICK_IsActiveCounterFlag(void)
{
	return ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == (SysTick_CTRL_COUNTFLAG_Msk));
}
uint64_t Sys::micros(void)
{
	/* Ensure COUNTFLAG is reset by reading SysTick control and status register */
	LL_SYSTICK_IsActiveCounterFlag();
	uint32_t m = HAL_GetTick();
	const uint32_t tms = SysTick->LOAD + 1;
	__IO uint32_t u = tms - SysTick->VAL;
	if (LL_SYSTICK_IsActiveCounterFlag())
	{
		m = HAL_GetTick();
		u = tms - SysTick->VAL;
	}
	return (m * 1000 + (u * 1000) / tms);
}
#else
uint64_t Sys::micros()
{
	return Sys::millis() * 1000;
}
#endif

std::string __hostname;

const char *Sys::hostname()
{
	return __hostname.c_str();
}
void Sys::hostname(const char *name) { __hostname = name; }
