/*
 * sys.cpp
 *
 *  Created on: Nov 7, 2021
 *      Author: lieven
 */
#ifdef __STM32__
#include "stm32f1xx_hal.h"
#endif
#include "Sys.h"

#include <string>

std::string __hostname;

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

#ifdef CPU_STM32
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

const char *Sys::hostname()
{
	return __hostname.c_str();
}
void Sys::hostname(const char *name) { __hostname = name; }
