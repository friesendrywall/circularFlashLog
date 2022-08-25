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
#define FLASH_ERASED (0xFF)
#define FLASH_MIN_BUFF 0x100
#define FLASH_SECTORS (FLASH_LOGS_LENGTH / FLASH_SECTOR_SIZE)
#define FLASH_UNCACHED_MALLOC_SIZE (FLASH_SECTOR_SIZE)

enum {
    CIRC_LOG_ERR_NONE,
    CIRC_LOG_ERR_NOT_INITIALIZED
};

extern uint32_t circFlashRead(uint32_t FlashAddress, uint8_t *buff,
                              uint32_t len);
extern uint32_t circFlashWrite(uint32_t FlashAddress, uint8_t *buff,
                               uint32_t len);
extern uint32_t circFlashErase(uint32_t FlashAddress, uint32_t len);

int circularWriteLog(unsigned char *buf, int len);
int32_t circularReadLogPartial(unsigned char *buff, int32_t seek,
                               int32_t desiredlen, int32_t *remaining);

#ifdef USE_STATIC_ALLOCATION

int circularLogInit(uint8_t *buf, uint32_t bufLen);

uint32_t circularReadLines(uint8_t *buff, uint32_t buffSize, uint32_t lines,
                           char *filter);
#else
int circularLogInit(void);
unsigned char *circularReadLog(uint32_t *len);
unsigned char *circularReadLines(uint32_t lines, uint32_t *outlen);
#endif



int32_t circularClearLog(void);

#endif