#ifndef __CIRCULARFLASH_H
#define __CIRCULARFLASH_H
#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>
	//Configuration defines
#define FLASH_LOGS_ADDRESS  0x200000
#define FLASH_LOGS_LENGTH   0x1E0000
#define FLASH_SECTOR_SIZE	0x1000
#define FLASH_WRITE_SIZE	0x100
#define FLASH_ERASED (0xFF)
#define FLASH_SECTORS (FLASH_LOGS_LENGTH/FLASH_SECTOR_SIZE)
#define FLASH_MUTEX_ENTER()
#define FLASH_MUTEX_EXIT()
#define FLASH_MALLOC malloc
#define FLASH_FREE free
#define FLASH_UNCACHED_MALLOC_SIZE (FLASH_SECTOR_SIZE)
#define LOG_MALLOC malloc
#define LOG_FREE free
#define FLASH_DEBUG printf
#define LINE_ESTIMATE_FACTOR 64

	extern uint32_t circFlashRead(uint32_t FlashAddress, unsigned char * buff, uint32_t len);
	extern uint32_t circFlashWrite(uint32_t FlashAddress, unsigned char * buff, uint32_t len);
	extern uint32_t circFlashErase(uint32_t FlashAddress, uint32_t len);

	int circularLogInit(void);
	int circularWriteLog(unsigned char * buf, int len);
	unsigned char* circularReadLog(uint32_t * len);
	int32_t circularReadLogPartial(unsigned char * buff, int32_t seek, int32_t desiredlen, int32_t * remaining);
	unsigned char * circularReadLines(uint32_t lines, uint32_t * outlen);
	int32_t circularClearLog(void);


#ifdef __cplusplus
}
#endif

#endif