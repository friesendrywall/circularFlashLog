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
// #include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "minunit.h"

#include "circularFlashConfig.h"
#include "src/circularflash.h"
#define FlashLogName "flashlog.bin"

int tests_run = 0;
int mutexCount = 0;

unsigned char *FakeFlash;
#define FLASH_LOGS_ADDRESS 0x200000
#define FLASH_LOGS_LENGTH 0x1E0000

uint32_t circFlashRead(uint32_t FlashAddress, uint8_t *buff,
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

uint32_t circFlashWrite(uint32_t FlashAddress, uint8_t *buff,
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

uint8_t wBuff[FLASH_WRITE_SIZE * 2];
circ_log_t log = {.name = "LOGS",
                  .read = circFlashRead,
                  .write = circFlashWrite,
                  .erase = circFlashErase,
                  .baseAddress = FLASH_LOGS_ADDRESS,
                  .logsLength = FLASH_LOGS_LENGTH,
                  .wBuff = wBuff,
                  .wBuffLen = sizeof(wBuff)};

void assertHandler(char *file, int line) {
  printf("CIRCULAR_LOG_ASSERT(%s:%i\r\n", file, line);
  int a = 0;
#ifdef _DEBUG
  while (1) {
    a++;
  }
#else
  exit(1);
#endif
}

static const char *test_circLogInit(void) {
  mu_assert("error, CIRC_LOG_ERR_NONE",
            circularLogInit(&log) == CIRC_LOG_ERR_NONE);
  mu_assert("error, mutex count", mutexCount == 0);
  return NULL;
}

static const char *test_newLogTest(void) {
  char printbuf[1024];
  uint8_t Read[LINE_ESTIMATE_FACTOR] = {0};
  time_t t = time(NULL);
  uint32_t len;
  len = sprintf(printbuf, "New log test at %i UTC\r\n", (int)t);
  circularWriteLog(&log, (unsigned char *)printbuf, len);
  circularReadLines(&log, Read, LINE_ESTIMATE_FACTOR, 1, NULL, 0);
  mu_assert("error, Doesn't match", memcmp(Read, printbuf, len) == 0);
  mu_assert("error, mutex count", mutexCount == 0);
  return NULL;
}

static const char *test_circLogWrap(void) {
  time_t t = time(NULL);
  uint32_t i;
  uint32_t len;
  uint8_t Read[LINE_ESTIMATE_FACTOR] = {0};
  static char printbuf[1024];
  len = sprintf(printbuf, "New log test at %i UTC\r\n", (int)t);
  circularWriteLog(&log, (unsigned char *)printbuf, len);
  circularReadLines(&log, Read, LINE_ESTIMATE_FACTOR, 1, NULL, 0);
  mu_assert("error, doesn't match", memcmp(Read, printbuf, len) == 0);
  for (i = 0; i < 100000; i++) {
    len = sprintf(printbuf, "Testing line %i to the log rand %i %i\r\n", i,
                  rand(), rand());
    circularWriteLog(&log, (uint8_t *)printbuf, len);
    circularReadLines(&log, Read, LINE_ESTIMATE_FACTOR, 1, NULL, 0);
    if (memcmp(Read, printbuf, len)) {
      sprintf(printbuf, "error, Line %i doesn't match, test failed", i);
      mu_assert(printbuf, 0);
    }
  }
  mu_assert("error, mutex count", mutexCount == 0);
  return NULL;
}

static const char *test_circLogShortMixed(void) {
  uint32_t i;
  uint32_t len;
  uint8_t Read[LINE_ESTIMATE_FACTOR * 10] = {0};
  static char printbuf[1024];
  for (i = 0; i < 10000; i++) {
    len = sprintf(printbuf, "Testing line %i to the log rand %i %i\r\n", i,
                  rand(), rand());
    circularWriteLog(&log, (unsigned char *)printbuf, len);
    len = sprintf(printbuf, "Testing line %i to the log rand %i %i\r\n", i,
                  rand(), rand());
    circularWriteLog(&log, (unsigned char *)printbuf, len);
    len = sprintf(printbuf, "Find something line %i unique %i %i\r\n", i,
                  rand(), rand());
    circularWriteLog(&log, (unsigned char *)printbuf, len);
    circularReadLines(&log, Read, LINE_ESTIMATE_FACTOR * 10, 10,
                      "Find something", 0);

    if (memcmp(Read, "Find something line", 19)) {
      sprintf(printbuf, "error, Line %i doesn't match, test failed", i);
      mu_assert(printbuf, 0);
      break;
    } 
  }
  mu_assert("error, mutex count", mutexCount == 0);
  return NULL;
}

static const char *test_circLogFileForward(void) {
  circular_FILE cf;
  uint32_t i;
  char printbuf[1024];
  uint8_t Read[1024];
  uint32_t len;
  for (i = 0; i < 1000; i++) {
    if (i % 100 == 0) {
      len = sprintf(printbuf, "Unique[%03i] test\r\n", i);
    } else {
      len = sprintf(printbuf, "Forward test line %i to the log rand %i %i\r\n", i,
                    rand(), rand());
    }
    circularWriteLog(&log, (unsigned char *)printbuf, len);
  }


  mu_assert("error, log file open err", circularFileOpen(&log, CIRC_FLAGS_OLDEST, &cf) ==
                    CIRC_LOG_ERR_NONE);

  len = circularFileRead(&log, &cf, Read, sizeof(Read), CIRC_DIR_FORWARD, 1000,
                         "Unique");
  mu_assert("error, Incorrect length",len == 18 * 10);
  mu_assert("error, Incorrect value", memcmp(Read, "Unique[000] test\r\n", 18) == 0);
  mu_assert("error, mutex count", mutexCount == 0);
  return NULL;
}

static const char *test_circLogFileReverse(void) {
  circular_FILE cf;
  uint32_t i;
  char printbuf[1024];
  uint8_t Read[1024];
  uint32_t len;
  for (i = 0; i < 1000; i++) {
    if (i % 100 == 0) {
      len = sprintf(printbuf, "Unique[%03i] test\r\n", i);
    } else {
      len = sprintf(printbuf, "Reverse[%i] to the log rand %i %i\r\n",
                    i, rand(), rand());
    }
    circularWriteLog(&log, (unsigned char *)printbuf, len);
  }

  mu_assert("error, log file open err #1",
            circularFileOpen(&log, CIRC_FLAGS_NEWEST, &cf) ==
                CIRC_LOG_ERR_NONE);

  len = circularFileRead(&log, &cf, Read, sizeof(Read), CIRC_DIR_REVERSE, 1000,
                         "Unique");
  mu_assert("error, Incorrect First value",
            memcmp(Read, "Unique[900] test\r\n", 18) == 0);
  mu_assert("error, Incorrect Second value",
            memcmp(&Read[18], "Unique[800] test\r\n", 18) == 0);
  mu_assert("error, mutex count", mutexCount == 0);
  /* seek filter */
  mu_assert("error, log file open err #2",
            circularFileOpen(&log, CIRC_FLAGS_NEWEST, &cf) ==
                CIRC_LOG_ERR_NONE);
  len = circularFileRead(&log, &cf, Read, sizeof(Read), CIRC_DIR_REVERSE, 1,
                         "seek8");
  Read[len] = 0;
  mu_assert("error, Incorrect Index[4] value",
            memcmp(Read, "Reverse[991]", 12) == 0);
  return NULL;
}

static const char *test_circLogSearchHang(void) {
  circular_FILE cf = {0};
  uint8_t Read[1024] = {0};
  uint32_t len;

  mu_assert("error, log file open err",
            circularFileOpen(&log, CIRC_FLAGS_NEWEST, &cf) ==
                CIRC_LOG_ERR_NONE);

  len = circularFileRead(&log, &cf, Read, sizeof(Read), CIRC_DIR_REVERSE, 1000,
                         "Bla-Bla");
  mu_assert("error, len != 0", len == 0);
  mu_assert("error, log file open err",
            circularFileOpen(&log, CIRC_FLAGS_OLDEST, &cf) ==
                CIRC_LOG_ERR_NONE);

  len = circularFileRead(&log, &cf, Read, sizeof(Read), CIRC_DIR_REVERSE, 1000,
                         "Bla-Bla");
  mu_assert("error, len != 0", len == 0);
  mu_assert("error, log file open err",
            circularFileOpen(&log, CIRC_FLAGS_NEWEST, &cf) ==
                CIRC_LOG_ERR_NONE);

  len = circularFileRead(&log, &cf, Read, sizeof(Read), CIRC_DIR_FORWARD, 1000,
                         "Bla-Bla");
  mu_assert("error, len != 0", len == 0);
  mu_assert("error, log file open err",
            circularFileOpen(&log, CIRC_FLAGS_OLDEST, &cf) ==
                CIRC_LOG_ERR_NONE);

  len = circularFileRead(&log, &cf, Read, sizeof(Read), CIRC_DIR_FORWARD, 1000,
                         "Bla-Bla");
  mu_assert("error, len != 0", len == 0);
  return NULL;
}

static const char *all_tests() {
  mu_run_test(test_circLogInit);
  mu_run_test(test_newLogTest);
  mu_run_test(test_circLogWrap);
  mu_run_test(test_circLogShortMixed);
  mu_run_test(test_circLogFileForward);
  mu_run_test(test_circLogFileReverse);
  mu_run_test(test_circLogSearchHang);
  return NULL;
}

int main(int argc, char *argv[]) {

  FakeFlash = (unsigned char *)malloc(FLASH_LOGS_LENGTH);
  if (FakeFlash == NULL) {
    return -1;
  }

  FILE *FF = fopen(FlashLogName, "rb");
  if (FF != NULL) {
    fread(FakeFlash, 1, FLASH_LOGS_LENGTH, FF);
    fclose(FF);
  } else {
    memset(FakeFlash, FLASH_ERASED, FLASH_LOGS_LENGTH);
  }

  const char *result = all_tests();
  if (result != NULL) {
    printf("%s\n", result);
  } else {
    printf("ALL TESTS PASSED\n");
  }
  printf("Tests run: %d\n", tests_run);
  /* persist memory here */
  FF = fopen(FlashLogName, "wb");
  if (FF != NULL) {
    fwrite(FakeFlash, 1, FLASH_LOGS_LENGTH, FF);
    fclose(FF);
  } else {
    printf("File IO error\r\n");
  }
  return (result != NULL);
}
