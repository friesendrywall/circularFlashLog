#ifndef __CIRCULARFLASHCONFIG_H
#define __CIRCULARFLASHCONFIG_H
#include <stdint.h>
// Configuration defines
#define FLASH_LOGS_ADDRESS 0x200000
#define FLASH_LOGS_LENGTH 0x1E0000
#define FLASH_SECTOR_SIZE 0x1000
#define FLASH_WRITE_SIZE 0x100
// Must be divisible by FLASH_WRITE_SIZE
#define STATIC_WRITE_BUFF_SIZE (FLASH_WRITE_SIZE * 2)

#define USE_STATIC_ALLOCATION

#define FLASH_MUTEX_ENTER()
#define FLASH_MUTEX_EXIT()
#ifndef USE_STATIC_ALLOCATION
#define FLASH_MALLOC malloc
#define FLASH_FREE free
#define LOG_MALLOC malloc
#define LOG_FREE free
#endif
#define FLASH_DEBUG printf
#define LINE_ESTIMATE_FACTOR 64
// #define LOG_CACHE_INVALIDATE(addr, len)
#endif
