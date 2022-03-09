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

uint64_t Sys::micros()
{
	return Sys::millis() * 1000;
}

const char *Sys::hostname() { return __hostname.c_str(); }
void Sys::hostname(const char *name) { __hostname = name; }
