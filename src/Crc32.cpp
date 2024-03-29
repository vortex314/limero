/*
 * Crc32.cpp
 *
 *  Created on: Nov 11, 2021
 *      Author: lieven
 */


#include <sys/types.h>
#include <stdint.h>
#include "Crc32.h"

uint32_t crc32(uint32_t crc, uint32_t data)
{
  int i;
  crc = crc ^ data;
  for(i=0; i<32; i++)
    if (crc & 0x80000000)
      crc = (crc << 1) ^ 0x04C11DB7; // Polynomial used in STM32
    else
      crc = (crc << 1);
  return(crc);
}

uint32_t crcSoft(uint32_t *data,size_t size){
	uint32_t crc = 0xFFFFFFFF;
	for( uint32_t i=0;i<size;i++) crc=crc32(crc,data[i]);
	return crc;

}


#ifdef __STM32__NO

#include "main.h"
#include "cmsis_os.h"
#include "Log.h"

extern "C" CRC_HandleTypeDef hcrc;

uint32_t Crc32::bufferCrc(uint32_t *in_buf, size_t in_buf_length) {
    __CRC_CLK_ENABLE();
    uint32_t result = HAL_CRC_Calculate(&hcrc, in_buf, in_buf_length);
    __HAL_RCC_CRC_CLK_DISABLE();
    return result;
}

#else 

uint32_t Crc32::bufferCrc(uint32_t *data,size_t size){
	uint32_t crc = 0xFFFFFFFF;
	for( uint32_t i=0;i<size;i++) crc=crc32(crc,data[i]);
	return crc;

}
#endif