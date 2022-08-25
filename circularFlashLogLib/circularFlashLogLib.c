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
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

/**
* Test framework
*/
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "circularFlashConfig.h"
#include "circularflash.h"
#define FlashLogName "flashlog.bin"

unsigned char *FakeFlash;
#define FLASH_LOGS_ADDRESS 0x200000
#define FLASH_LOGS_LENGTH 0x1E0000

uint32_t circFlashRead(uint32_t FlashAddress, unsigned char *buff,
                       uint32_t len) {
  if (FlashAddress < FLASH_LOGS_ADDRESS ||
      FlashAddress >= FLASH_LOGS_ADDRESS + FLASH_LOGS_LENGTH) {
    printf("Address out of range 0x%X\r\n", FlashAddress);
    return 0;
  }
  if (FlashAddress + len > FLASH_LOGS_ADDRESS + FLASH_LOGS_LENGTH) {
    printf("Address+len out of range 0x%X\r\n", FlashAddress);
    return 0;
  }
  memcpy(buff, &FakeFlash[FlashAddress - FLASH_LOGS_ADDRESS], len);
  return len;
}

uint32_t circFlashWrite(uint32_t FlashAddress, unsigned char *buff,
                        uint32_t len) {
  uint32_t i;
  if (FlashAddress < FLASH_LOGS_ADDRESS ||
      FlashAddress >= FLASH_LOGS_ADDRESS + FLASH_LOGS_LENGTH) {
    printf("Address out of range 0x%X\r\n", FlashAddress);
    return 0;
  }
  if (FlashAddress + len > FLASH_LOGS_ADDRESS + FLASH_LOGS_LENGTH) {
    printf("Address+len out of range 0x%X\r\n", FlashAddress);
    return 0;
  }
  for (i = 0; i < len; i++) {
    FakeFlash[FlashAddress - FLASH_LOGS_ADDRESS + i] &= buff[i];
  }
  return len;
}

uint32_t circFlashErase(uint32_t FlashAddress, uint32_t len) {
  if (FlashAddress < FLASH_LOGS_ADDRESS ||
      FlashAddress >= FLASH_LOGS_ADDRESS + FLASH_LOGS_LENGTH) {
    printf("Address out of range 0x%X\r\n", FlashAddress);
    return 0;
  }
  if (FlashAddress + len > FLASH_LOGS_ADDRESS + FLASH_LOGS_LENGTH) {
    printf("Address+len out of range 0x%X\r\n", FlashAddress);
    return 0;
  }
  memset(&FakeFlash[FlashAddress - FLASH_LOGS_ADDRESS], FLASH_ERASED, len);
  return len;
}


int main(int argc, char *argv[]) {
  uint32_t i;
  int32_t rem;
  uint8_t wBuff[FLASH_WRITE_SIZE];
  circ_log_t log = {.name = "LOGS",
                    .baseAddress = FLASH_LOGS_ADDRESS,
                    .logsLength = FLASH_LOGS_LENGTH,
                    .wBuff = wBuff,
                    .wBuffLen = FLASH_WRITE_SIZE};

  FakeFlash = (unsigned char *)malloc(FLASH_LOGS_LENGTH);
  if (FakeFlash == NULL) {
    return -1;
  }
  unsigned char *Read;
  FILE *FF = fopen(FlashLogName, "rb");
  if (FF != NULL) {
    fread(FakeFlash, 1, FLASH_LOGS_LENGTH, FF);
    fclose(FF);
  } else {
    memset(FakeFlash, FLASH_ERASED, FLASH_LOGS_LENGTH);
  }
#ifdef USE_STATIC_ALLOCATION
  uint8_t *workBuff = malloc(FLASH_SECTOR_SIZE);
  if (workBuff == NULL) {
    return -1;
  }
  // TODO: try different sizes
  if (!circularLogInit(&log, workBuff, FLASH_SECTOR_SIZE)) {
    printf("Init error\r\n");
    return -1;
  };
#else
  if (!circularLogInit(&log)) {
    printf("Init error\r\n");
    return -1;
  };
#endif
  
  time_t t = time(NULL);

  uint32_t remaining, index, len;

  char *printbuf = (char *)malloc(1024);
  if (printbuf == NULL) {
    return -1;
  }
  len = sprintf(printbuf, "New log test at %i UTC\r\n", (int)t);
  circularWriteLog(&log, (unsigned char *)printbuf, len);
#ifdef USE_STATIC_ALLOCATION
  Read = malloc(LINE_ESTIMATE_FACTOR);
  if (Read == NULL) {
    return -1;
  }
  circularReadLines(&log, Read, LINE_ESTIMATE_FACTOR, 1, NULL);
#else
  Read = circularReadLines(&log, 1, &len);
#endif
  if (memcmp(Read, printbuf, len)) {
    printf("Doesn't match\r\n");
  }
  free(Read);
  for (i = 0; i < 100000; i++) {
    len = sprintf(printbuf, "Testing line %i to the log rand %i %i\r\n", i,
                  rand(), rand());
    circularWriteLog(&log, (unsigned char *)printbuf, len);
#ifdef USE_STATIC_ALLOCATION
    Read = malloc(LINE_ESTIMATE_FACTOR);
    if (Read == NULL) {
      return -1;
    }
    circularReadLines(&log, Read, LINE_ESTIMATE_FACTOR, 1, NULL);
#else
    Read = circularReadLines(&log, 1, &len);
#endif
    if (memcmp(Read, printbuf, len)) {
      printf("Line %i doesn't match, test failed\r\n", i);
      free(Read);
      break;
    } else {
      printf("\rLine %i passed:", i);
    }
    free(Read);
  }

  for (i = 0; i < 100; i++) {
    len = sprintf(printbuf, "Testing line %i to the log rand %i %i\r\n", i,
                  rand(), rand());
    circularWriteLog(&log, (unsigned char *)printbuf, len);
    len = sprintf(printbuf, "Testing line %i to the log rand %i %i\r\n", i,
                  rand(), rand());
    circularWriteLog(&log, (unsigned char *)printbuf, len);
    len = sprintf(printbuf, "Find something line %i unique %i %i\r\n", i,
                  rand(), rand());
    circularWriteLog(&log, (unsigned char *)printbuf, len);
#ifdef USE_STATIC_ALLOCATION
    Read = malloc(LINE_ESTIMATE_FACTOR * 10);
    if (Read == NULL) {
      return -1;
    }
    circularReadLines(&log, Read, LINE_ESTIMATE_FACTOR * 10, 10,
                      "Find something");
#else
    Read = circularReadLines(&log, 1, &len);
#endif
    if (memcmp(Read, "Find something line", 19)) {
      printf("Line %i doesn't match, test failed\r\n", i);
      free(Read);
      break;
    } else {
      printf("\rLine %i passed:", i);
    }
    free(Read);
  }

  FF = fopen(FlashLogName, "wb");
  if (FF != NULL) {
    fwrite(FakeFlash, 1, FLASH_LOGS_LENGTH, FF);
    fclose(FF);
  } else {
    printf("File IO error\r\n");
  }

  return 0;
}
