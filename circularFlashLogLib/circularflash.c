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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "circularFlashConfig.h"
#include "circularflash.h"

static long LogFlashTailPtr = -1;
static long LogFlashHeadPtr = -1;
static unsigned int circLogInit = 0;

static int32_t calculateErasedSpace(void) {
  if (LogFlashTailPtr == 0 && LogFlashHeadPtr == 0) {
    return FLASH_LOGS_LENGTH; // Never written, new flash
  } else if (LogFlashTailPtr == -1 && LogFlashHeadPtr == -1) {
    FLASH_DEBUG("FLASH: Log corrupted\r\n");
    return 0;
  } else if (LogFlashHeadPtr > LogFlashTailPtr) {
    //     FlashLength - (used space)
    return FLASH_LOGS_LENGTH - (LogFlashHeadPtr - LogFlashTailPtr);
  } else if (LogFlashHeadPtr < LogFlashTailPtr) {
    //       Wrapped
    //       FlashLength     - (  Used          + (     used ))
    return FLASH_LOGS_LENGTH -
           (LogFlashHeadPtr + (FLASH_LOGS_LENGTH - LogFlashTailPtr));
  } else {
    FLASH_DEBUG("FLASH: Log corrupted\r\n");
    return 0;
  }
}

static int32_t calculateLogSpace(void) {
  if (LogFlashTailPtr == 0 && LogFlashHeadPtr == 0) {
    return 0; // Never written, new flash
  } else if (LogFlashTailPtr == -1 && LogFlashHeadPtr == -1) {
    FLASH_DEBUG("FLASH: Log corrupted\r\n");
    return 0;
  } else if (LogFlashHeadPtr > LogFlashTailPtr) {
    return (LogFlashHeadPtr - LogFlashTailPtr);
  } else if (LogFlashHeadPtr < LogFlashTailPtr) {
    return LogFlashHeadPtr + (FLASH_LOGS_LENGTH - LogFlashTailPtr);
  } else {
    FLASH_DEBUG("FLASH: Log corrupted\r\n");
    return 0;
  }
}

/* This function inserts, assuming that writing 1's won't change anything*/
static uint32_t circFlashInsertWrite(uint32_t FlashAddress, unsigned char *buff,
                                     uint32_t len) {
  uint32_t rem, end, begin, WriteLen, res;
  unsigned char *tbuff;
  rem = FlashAddress % FLASH_WRITE_SIZE;
  begin = FlashAddress - rem;
  end = FlashAddress + len; // Extend up to boundary
  if (end % FLASH_WRITE_SIZE) {
    WriteLen = (((end - begin) / FLASH_WRITE_SIZE) + 1) * FLASH_WRITE_SIZE;
  } else {
    WriteLen = (((end - begin) / FLASH_WRITE_SIZE)) * FLASH_WRITE_SIZE;
  }
  tbuff = FLASH_MALLOC(WriteLen);
  if (tbuff == NULL) {
    FLASH_DEBUG("FLASH: FLASH_MALLOC error\r\n");
    return 0;
  }
  memset(tbuff, FLASH_ERASED, WriteLen);
  memcpy(&tbuff[rem], buff, len);
  res = circFlashWrite(begin, tbuff, WriteLen);
  FLASH_FREE(tbuff);
  return res == WriteLen ? len : 0;
}

int32_t circularReadLogPartial(unsigned char *buff, int32_t seek,
                               int32_t desiredlen, int32_t *remaining) {
  int32_t ret = 0;
  int32_t res, firstlen, secondlen;
  if (!circLogInit) {
    return 0;
  }
  FLASH_MUTEX_ENTER();
  int space = calculateLogSpace();
  if (desiredlen > (space - seek)) {
    desiredlen = (space - seek);
  }
  if (space > 0 && desiredlen > 0) {
    if (LogFlashHeadPtr > LogFlashTailPtr) {
      res = circFlashRead(FLASH_LOGS_ADDRESS + LogFlashTailPtr + seek, buff,
                          desiredlen);
      if (res != desiredlen) {
        FLASH_DEBUG("FLASH: IO error\r\n");
        ret = 0;
        *remaining = 0;
        goto badexit;
      }
      *remaining = (space - seek - desiredlen);
      ret = desiredlen;
    } else if (LogFlashHeadPtr < LogFlashTailPtr) {
      // See which part the caller wants
      firstlen = FLASH_LOGS_LENGTH - LogFlashTailPtr;
      if (seek > firstlen) {
        // The upper half of the request
        res = circFlashRead(FLASH_LOGS_ADDRESS + (seek - firstlen), buff,
                            desiredlen);
        if (res != desiredlen) {
          FLASH_DEBUG("FLASH: IO error\r\n");
          ret = 0;
          *remaining = 0;
          goto badexit;
        }
        ret = desiredlen;
        *remaining = (space - seek - desiredlen);
      } else {
        // Lower half first from end of address space
        if (seek + desiredlen + LogFlashTailPtr > FLASH_LOGS_LENGTH) {
          secondlen = FLASH_LOGS_LENGTH - (LogFlashTailPtr + seek);
          res = circFlashRead(FLASH_LOGS_ADDRESS + LogFlashTailPtr + seek, buff,
                              secondlen);
          if (res != secondlen) {
            FLASH_DEBUG("FLASH: IO error\r\n");
            ret = 0;
            *remaining = 0;
            goto badexit;
          }
          res = circFlashRead(FLASH_LOGS_ADDRESS, &buff[secondlen],
                              desiredlen - secondlen);
          if (res != desiredlen - secondlen) {
            FLASH_DEBUG("FLASH: IO error\r\n");
            ret = 0;
            *remaining = 0;
            goto badexit;
          }
          ret = desiredlen;
          *remaining = (space - seek - desiredlen);
        } else {
          res = circFlashRead(FLASH_LOGS_ADDRESS + LogFlashTailPtr + seek, buff,
                              desiredlen);
          if (res != desiredlen) {
            FLASH_DEBUG("FLASH: IO error\r\n");
          }
          ret = res;
          *remaining = (space - seek - desiredlen);
        }
      }
    }
  } else {
    ret = 0;
    *remaining = 0;
  }
badexit:
  FLASH_MUTEX_EXIT();
  return ret;
}

/*
 * Returns a LOG_MALLOC'ed pointer to the log
 */
unsigned char *circularReadLog(uint32_t *len) {
  unsigned char *ret = NULL;
  uint32_t res, firstlen;
  static char NoLogs[] = "Empty log";
  if (!circLogInit) {
    *len = 0;
    return NULL;
  }
  FLASH_MUTEX_ENTER();
  int space = calculateLogSpace();
  if (space > 0) {
    ret = LOG_MALLOC(space + 1);
    if (ret == NULL) {
      FLASH_DEBUG("FLASH: Malloc error\r\n");
      goto badexit;
    }
    if (LogFlashHeadPtr > LogFlashTailPtr) {
      LOG_CACHE_INVALIDATE(ret, space + 1);	
      res = circFlashRead(FLASH_LOGS_ADDRESS + LogFlashTailPtr, ret, space);
      *len = res + 1;
      if (res != space) {
        FLASH_DEBUG("FLASH: IO error\r\n");
      }
      ret[res] = 0;
    } else if (LogFlashHeadPtr < LogFlashTailPtr) {
      firstlen = FLASH_LOGS_LENGTH - LogFlashTailPtr;
      LOG_CACHE_INVALIDATE(ret, space + 1);	
      res = circFlashRead(FLASH_LOGS_ADDRESS + LogFlashTailPtr, ret, firstlen);
      if (res != firstlen) {
        FLASH_DEBUG("FLASH: IO error\r\n");
      }
      res = circFlashRead(FLASH_LOGS_ADDRESS, &ret[firstlen], LogFlashHeadPtr);
      if (res != LogFlashHeadPtr) {
        FLASH_DEBUG("FLASH: IO error\r\n");
      }
      *len = space + 1;
      ret[space] = 0;
    }
  } else {
    ret = LOG_MALLOC(sizeof(NoLogs));
    if (ret) {
      *len = sizeof(NoLogs);
      memcpy(ret, NoLogs, sizeof(NoLogs));
    }
  }
badexit:
  FLASH_MUTEX_EXIT();
  return ret;
}

/*
 * Returns a LOG_MALLOC'ed pointer to the number of log lines requested
 */
unsigned char *circularReadLines(uint32_t lines, uint32_t *outlen) {
  uint32_t llen = 0;
  int32_t i, seek;
  uint32_t t;
  unsigned char *ret;
  if (!lines) {
    *outlen = 0;
    return 0;
  }
  lines++;
  if (!circLogInit) {
    *outlen = 0;
    return NULL;
  }
  unsigned char *tmp = LOG_MALLOC(LINE_ESTIMATE_FACTOR * lines);
  if (tmp) {
    FLASH_MUTEX_ENTER();
    seek = calculateLogSpace();
    seek -= LINE_ESTIMATE_FACTOR * lines;
    if (seek < 0) {
      seek = 0;
    }
    FLASH_MUTEX_EXIT();
    LOG_CACHE_INVALIDATE(tmp, LINE_ESTIMATE_FACTOR * lines);
    llen = circularReadLogPartial(tmp, seek, LINE_ESTIMATE_FACTOR * lines, &i);
    for (i = llen - 1; i >= 0; i--) {
      if (tmp[i] == '\n') {
        lines--;
        if (lines == 0) {
          i++;
          t = llen - i;
          ret = LOG_MALLOC(t);
          memcpy(ret, &tmp[i], t);
          LOG_FREE(tmp);
          *outlen = t;
          return ret;
        }
      } else if(tmp[i] < 0x0A){
        tmp[i] = '?';
      }
    }
    *outlen = llen;
    return tmp;
  } else {
    *outlen = 0;
  }
  return NULL;
}

int32_t circularClearLog(void) {
  FLASH_MUTEX_ENTER();
  if (circFlashErase(FLASH_LOGS_ADDRESS, FLASH_LOGS_LENGTH) !=
      FLASH_LOGS_LENGTH) {
    FLASH_DEBUG("FLASH: Erase IO error\r\n");
    goto badexit;
  }
  FLASH_DEBUG("FLASH: Entire flash erased\r\n");
  LogFlashTailPtr = LogFlashHeadPtr = 0;
  FLASH_MUTEX_EXIT();
  return 1;
badexit:
  FLASH_MUTEX_EXIT();
  return 0;
}

/*
 *
 */
int circularWriteLog(unsigned char *buf, int len) {
  int32_t EraseSpace;
  uint32_t res, firstlen;
  if (len > FLASH_SECTOR_SIZE) {
    len = FLASH_SECTOR_SIZE;
  }
  FLASH_MUTEX_ENTER();
  EraseSpace = calculateErasedSpace();
  if (EraseSpace == 0) {
    // Erase it all
    if (circFlashErase(FLASH_LOGS_ADDRESS, FLASH_LOGS_LENGTH) !=
        FLASH_LOGS_LENGTH) {
      FLASH_DEBUG("FLASH: Erase IO error\r\n");
      goto badexit;
    }
    FLASH_DEBUG("FLASH: Entire flash erased\r\n");
    LogFlashTailPtr = LogFlashHeadPtr = 0;
  } else if (EraseSpace < (FLASH_SECTOR_SIZE * 2)) {
    // Erase next sector in line
    if (circFlashErase(FLASH_LOGS_ADDRESS + LogFlashTailPtr,
                       FLASH_SECTOR_SIZE) != FLASH_SECTOR_SIZE) {
      FLASH_DEBUG("FLASH: Erase IO error\r\n");
      goto badexit;
    }
    FLASH_DEBUG("FLASH: Sector at address 0x%X erased\r\n",
                FLASH_LOGS_ADDRESS + LogFlashTailPtr);
    LogFlashTailPtr += FLASH_SECTOR_SIZE;
    if (LogFlashTailPtr >= FLASH_LOGS_LENGTH) {
      LogFlashTailPtr = 0;
    }
  }
  // Does it wrap?
  // The FLASH_WRITE_SIZE boundary will assume that writing FLASH_ERASED will
  // leave existing alone
  if (LogFlashHeadPtr + len > FLASH_LOGS_LENGTH) {
    // Wrapped
    firstlen = FLASH_LOGS_LENGTH - LogFlashHeadPtr;
    res = circFlashInsertWrite(FLASH_LOGS_ADDRESS + LogFlashHeadPtr, buf,
                               firstlen);
    if (res != firstlen) {
      FLASH_DEBUG("FLASH: Write IO error\r\n");
      goto badexit;
    }
    res = circFlashInsertWrite(FLASH_LOGS_ADDRESS, &buf[firstlen],
                               len - firstlen);
    if (res != len - firstlen) {
      FLASH_DEBUG("FLASH: Write IO error\r\n");
      goto badexit;
    }
    LogFlashHeadPtr = len - firstlen;
  } else {
    res = circFlashInsertWrite(FLASH_LOGS_ADDRESS + LogFlashHeadPtr, buf, len);
    if (res != len) {
      FLASH_DEBUG("FLASH: Write IO error\r\n");
      goto badexit;
    }
    LogFlashHeadPtr += len;
  }

  FLASH_MUTEX_EXIT();
  return 1;
badexit:
  FLASH_MUTEX_EXIT();
  return 0;
}

int circularLogInit(void) {
  uint32_t res, i, si;
  LogFlashTailPtr = -1;
  LogFlashHeadPtr = -1;

  unsigned char *buf = FLASH_MALLOC(FLASH_SECTOR_SIZE);
  if (buf == NULL) {
    return 0;
  }
  res = circFlashRead(FLASH_LOGS_ADDRESS, buf, 4);
  if (res != 4) {
    goto badexit;
  }

  if (buf[0] == FLASH_ERASED) {
    // Search for tail first
    for (i = 1; i < FLASH_SECTORS; i++) {
      res = circFlashRead(FLASH_LOGS_ADDRESS + (FLASH_SECTOR_SIZE * i), buf, 4);
      if (res != 4) {
        goto badexit;
      }

      if (buf[0] != FLASH_ERASED) {
        LogFlashTailPtr = i * FLASH_SECTOR_SIZE;
        break;
      }
    }
    if (LogFlashTailPtr == -1) {
      // Device is empty
      FLASH_DEBUG("FLASH: Device is empty\r\n");
      LogFlashTailPtr = 0;
      LogFlashHeadPtr = 0;
      goto goodexit;
    }
    // Now search for head
    for (i = LogFlashTailPtr; i < FLASH_LOGS_LENGTH; i += FLASH_SECTOR_SIZE) {
      res = circFlashRead(FLASH_LOGS_ADDRESS + i, buf, FLASH_SECTOR_SIZE);
      if (res != FLASH_SECTOR_SIZE) {
        goto badexit;
      }
      for (si = 0; si < FLASH_SECTOR_SIZE; si++) {
        if (buf[si] == FLASH_ERASED) {
          LogFlashHeadPtr = i + si;
          break;
        }
      }
      if (LogFlashHeadPtr != -1) {
        break;
      }
    }
    if (LogFlashHeadPtr == -1) {
      // This would only happen if head is at 0
      LogFlashHeadPtr = 0;
    }

  } else {
    // Search for head first
    for (i = 0; i < FLASH_LOGS_LENGTH; i += FLASH_SECTOR_SIZE) {
      res = circFlashRead(FLASH_LOGS_ADDRESS + i, buf, FLASH_SECTOR_SIZE);
      if (res != FLASH_SECTOR_SIZE) {
        goto badexit;
      }
      for (si = 0; si < FLASH_SECTOR_SIZE; si++) {
        if (buf[si] == FLASH_ERASED) {
          LogFlashHeadPtr = i + si;
          break;
        }
      }
      if (LogFlashHeadPtr != -1) {
        break;
      }
    }

    if (LogFlashHeadPtr == -1) {
      // Device is full
      FLASH_DEBUG("FLASH: Device is full\r\n");
      LogFlashTailPtr = -1;
      LogFlashHeadPtr = -1;
      goto goodexit;
    }

    // Now search for tail
    for (i = (LogFlashHeadPtr / FLASH_SECTOR_SIZE) + 1; i < FLASH_SECTORS;
         i++) {
      res = circFlashRead(FLASH_LOGS_ADDRESS + (FLASH_SECTOR_SIZE * i), buf, 4);
      if (res != 4) {
        goto badexit;
      }
      if (buf[0] != FLASH_ERASED) {
        LogFlashTailPtr = i * FLASH_SECTOR_SIZE;
        break;
      }
    }
    if (LogFlashTailPtr == -1) {
      // This would only happen if tail is 0
      LogFlashTailPtr = 0;
    }
  }
goodexit:
  FLASH_DEBUG("FLASH: Head is at 0x%X\r\n", LogFlashHeadPtr);
  FLASH_DEBUG("FLASH: Tail is at 0x%X\r\n", LogFlashTailPtr);
  FLASH_DEBUG("FLASH: Erased --  0x%X\r\n", calculateErasedSpace());
  FLASH_DEBUG("FLASH: Logs ----  0x%X\r\n", calculateLogSpace());
  FLASH_FREE(buf);
  circLogInit = 1;
  return 1;

badexit:
  FLASH_DEBUG("FLASH: Device error\r\n");
  FLASH_FREE(buf);
  return 0;
}
