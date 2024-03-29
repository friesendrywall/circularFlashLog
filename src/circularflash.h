/**
 * MIT License
 *
 * Copyright (c) 2022 Erik Friesen
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef __CIRCULARFLASH_H
#define __CIRCULARFLASH_H

#include <stdint.h>
#include "circularFlashConfig.h"

#define CIRCULAR_FLASH_VERSION "1.05"

#define FLASH_ERASED (0xFF)
#define FLASH_SECTORS(logLength) (logLength / FLASH_SECTOR_SIZE)
#define LINES_READ_ALL (-1)

#ifndef FLASH_MAX_DATE_LEN
#define FLASH_MAX_DATE_LEN 32
#endif

#ifndef FLASH_DEBUG
#define FLASH_DEBUG(...)
#endif

#ifndef LINE_ESTIMATE_FACTOR
#define LINE_ESTIMATE_FACTOR 64
#endif

#ifndef SEARCH_BUFF_SIZE
#define SEARCH_BUFF_SIZE 1024
#endif

#ifndef FLASH_SECTOR_SIZE
#define FLASH_SECTOR_SIZE 0x1000
#endif

#ifndef FLASH_WRITE_SIZE
#define FLASH_WRITE_SIZE 0x100
#endif

#if FLASH_MAX_DATE_LEN >= FLASH_WRITE_SIZE
#error "FLASH_MAX_DATE_LEN too long"
#endif

#define FLASH_MIN_BUFF (FLASH_WRITE_SIZE + FLASH_MAX_DATE_LEN)

/* Optional index, must be sector count */
typedef struct {
  uint32_t time;
  uint32_t firstLine;
} circ_log_index_t;

typedef struct {
  const char *name;
  const uint32_t baseAddress;
  const uint32_t logsLength;
  uint8_t * wBuff;
  const uint32_t wBuffLen;
  circ_log_index_t *index;
  void *osMutex;
  int32_t LogFlashTailPtr;
  int32_t LogFlashHeadPtr;
  uint8_t circLogInit : 1;
  uint8_t emptyFlag : 1;
  uint32_t (*read)(uint32_t FlashAddress, uint8_t *buff, uint32_t len);
  uint32_t (*write)(uint32_t FlashAddress, uint8_t *buff, uint32_t len);
  uint32_t (*erase)(uint32_t FlashAddress, uint32_t len);
  uint32_t (*parseTime)(const char * line);
} circ_log_t;

enum { 
    CIRC_LOG_ERR_NONE, 
    CIRC_LOG_ERR_IO, 
    CIRC_LOG_ERR_API,
    CIRC_LOG_ERR_INIT
};

typedef enum { 
    CIRC_FLAGS_OLDEST, 
    CIRC_FLAGS_NEWEST 
} CIRC_FLAGS;

typedef enum {
    CIRC_DIR_FORWARD,
    CIRC_DIR_REVERSE
} CIRC_DIR;

typedef struct {
  uint32_t seekPos;
  uint32_t headPtr;
  uint32_t tailPtr;
  uint32_t valid;
  CIRC_FLAGS flags;
  /* Search buffer */
  uint8_t wBuff[SEARCH_BUFF_SIZE];
} circular_FILE;

uint32_t circularLogInit(circ_log_t *log);
uint32_t circularClearLog(circ_log_t *log);
uint32_t circularWriteLog(circ_log_t *log, uint8_t *buf, uint32_t len);
uint32_t circularReadLogPartial(circ_log_t *log, uint8_t *buff,
                               uint32_t seek, uint32_t desiredlen, uint32_t *remaining);

uint32_t circularReadLines(circ_log_t *log, uint8_t *buff, uint32_t buffSize,
                           uint32_t lines, char *filter,
                           uint32_t estLineLength);

uint32_t circularFileOpen(circ_log_t *log, CIRC_FLAGS flags,
                          circular_FILE *file);

uint32_t circularFileRead(circ_log_t *log, circular_FILE *file, void *buff,
                          uint32_t buffLen, CIRC_DIR dir, int32_t lines,
                          char *filter);

uint32_t indexedLogSearch(circ_log_t *log, void *buff, uint32_t buffLen,
                          uint32_t time);

#endif