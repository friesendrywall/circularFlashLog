#ifndef __CIRCULARFLASHCONFIG_H
#define __CIRCULARFLASHCONFIG_H
#define FLASH_SECTOR_SIZE 0x1000
#define FLASH_WRITE_SIZE 0x100

extern int mutexCount;

#define FLASH_MUTEX_ENTER(x) mutexCount++
#define FLASH_MUTEX_EXIT(x) mutexCount--

#define FLASH_DEBUG printf
#define LINE_ESTIMATE_FACTOR 64
#define SEARCH_BUFF_SIZE 1024

void assertHandler(char *file, int line);
#define CIRCULAR_LOG_ASSERT(expr)                                              \
  if (!(expr))                                                                 \
  assertHandler(__FILE__, __LINE__)

#endif
