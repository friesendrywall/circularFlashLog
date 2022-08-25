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

static int32_t LogFlashTailPtr = -1;
static int32_t LogFlashHeadPtr = -1;
static uint32_t circLogInit = 0;

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
  static uint8_t tbuff[FLASH_WRITE_SIZE * 2];
  rem = FlashAddress % FLASH_WRITE_SIZE;
  begin = FlashAddress - rem;
  end = FlashAddress + len; // Extend up to boundary
  if (end % FLASH_WRITE_SIZE) {
    WriteLen = (((end - begin) / FLASH_WRITE_SIZE) + 1) * FLASH_WRITE_SIZE;
  } else {
    WriteLen = (((end - begin) / FLASH_WRITE_SIZE)) * FLASH_WRITE_SIZE;
  }
  if (tbuff == NULL) {
    FLASH_DEBUG("FLASH: FLASH_MALLOC error\r\n");
    return 0;
  }
  memset(tbuff, FLASH_ERASED, WriteLen);
  memcpy(&tbuff[rem], buff, len);
  res = circFlashWrite(begin, tbuff, WriteLen);
  return res == WriteLen ? len : 0;
}

/**
* seek = Bytes from start of log
*/
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

#ifdef USE_STATIC_ALLOCATION

uint32_t circularOpenLog(LOG_FILE *logFile) {
  unsigned char *ret = NULL;
  memset(&logFile, 0, sizeof(LOG_FILE));
  if (!circLogInit) {
    logFile->length = 0;
    return CIRC_LOG_ERR_NOT_INITIALIZED;
  }
  FLASH_MUTEX_ENTER();
  int space = calculateLogSpace();
  if (space > 0) {
    logFile->headPtr = LogFlashHeadPtr;
    logFile->tailPtr = LogFlashTailPtr;
    logFile->length = space;
    return CIRC_LOG_ERR_NONE;
  } else {
    return CIRC_LOG_ERR_NONE;
  }
}

uint32_t circularReadLog(LOG_FILE *logFile, uint8_t *buff, uint32_t len) {
  uint32_t res, firstlen, reqLen;
  if (len > logFile->length) {
    len = logFile->length;
  }
  if (len == 0) {
    return 0;
  }
  if (logFile->headPtr > logFile->tailPtr) {
#ifdef LOG_CACHE_INVALIDATE
    LOG_CACHE_INVALIDATE(buff, len);
#endif
    res = circFlashRead(FLASH_LOGS_ADDRESS + logFile->tailPtr, buff, len);
    logFile->tailPtr += len;
    logFile->length -= len;
    if (res != len) {
      FLASH_DEBUG("FLASH: IO error\r\n");
    }
    return len;
  } else if (LogFlashHeadPtr < LogFlashTailPtr) {
    reqLen = len;
    firstlen = FLASH_LOGS_LENGTH - logFile->tailPtr;
#ifdef LOG_CACHE_INVALIDATE
    LOG_CACHE_INVALIDATE(buff, len);
#endif
    if (len <= firstlen) {
      res = circFlashRead(FLASH_LOGS_ADDRESS + logFile->tailPtr, buff, len);
      if (res != len) {
        FLASH_DEBUG("FLASH: IO error\r\n");
      }
      logFile->tailPtr += len;
      if (logFile->tailPtr == FLASH_LOGS_LENGTH) {
        logFile->tailPtr = 0;
      }
      logFile->length -= len;
      return len;
    }
    res = circFlashRead(FLASH_LOGS_ADDRESS + logFile->tailPtr, buff, firstlen);
    if (res != firstlen) {
      FLASH_DEBUG("FLASH: IO error\r\n");
    }
    logFile->length -= firstlen;
    len -= firstlen;
    if (len > (uint32_t)logFile->headPtr) {
      len = logFile->headPtr;
    }
    res = circFlashRead(FLASH_LOGS_ADDRESS, &buff[firstlen], len);
    if (res != len) {
      FLASH_DEBUG("FLASH: IO error\r\n");
    }
    logFile->tailPtr = len;
    return len;
  } else {
    return 0;
  }
}

uint32_t circularReadLines(uint8_t *buff, uint32_t buffSize, uint32_t lines,
                           char *filter) {
  int32_t ret = 0;
  int32_t retSize = 0;
  int32_t remaining;
  int32_t space, seek;
  int32_t i, llen, searchLen;
  if (buffSize < LINE_ESTIMATE_FACTOR) {
    // Minimum buffSize of LINE_ESTIMATE_FACTOR required
    return 0;
  }
  FLASH_MUTEX_ENTER();
  space = calculateLogSpace();
  FLASH_MUTEX_EXIT();
  searchLen = lines * LINE_ESTIMATE_FACTOR;
  if (searchLen > buffSize - 1) {
    searchLen = buffSize - 1;
  }
  seek = space - searchLen;
  if (seek < 0) {
    seek = 0;
  }

  ret = circularReadLogPartial(buff, seek, searchLen, &remaining);
  if (ret == 0) {
    return 0;
  }

  // Search for "\n"
  for (i = ret - 3; i >= 0; i--) {
    if (buff[i] == '\n') {
      lines--;
    }
    if (lines == 0) {
      ret -= (i + 1);
      memcpy(buff, &buff[i + 1], ret);
      buff[ret] = 0;
      break;
    }
  }

  if (filter != NULL) {
    uint32_t FoundLength = 0;
    uint8_t *LastLine = buff;
    // Separate into lines
    for (i = 0; i < ret; i++) {
      if (buff[i] == '\n') {
        buff[i] = 0;
        if (strstr(LastLine, filter) != NULL) {
          buff[i] = '\n';
          llen = (&buff[i] - LastLine) + 1;
          memcpy(&buff[FoundLength], LastLine, llen);
          FoundLength += llen;
          buff[FoundLength] = 0;
        }
        buff[i] = '\n';
        LastLine = &buff[i + 1];
      }
    }
    if (FoundLength == 0) {
      snprintf(buff, buffSize,
               "** Search item '%s' not found in %i lines **\r\n", filter,
               lines);
      ret = strlen(buff);
    }
  }
  return ret;
}

#else
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
#ifdef LOG_CACHE_INVALIDATE
      LOG_CACHE_INVALIDATE(ret, space + 1);
#endif
      res = circFlashRead(FLASH_LOGS_ADDRESS + LogFlashTailPtr, ret, space);
      *len = res + 1;
      if (res != space) {
        FLASH_DEBUG("FLASH: IO error\r\n");
      }
      ret[res] = 0;
    } else if (LogFlashHeadPtr < LogFlashTailPtr) {
      firstlen = FLASH_LOGS_LENGTH - LogFlashTailPtr;
#ifdef LOG_CACHE_INVALIDATE
      LOG_CACHE_INVALIDATE(ret, space + 1);
#endif
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
#ifdef LOG_CACHE_INVALIDATE
    LOG_CACHE_INVALIDATE(tmp, LINE_ESTIMATE_FACTOR * lines);
#endif
    llen = circularReadLogPartial(tmp, seek, LINE_ESTIMATE_FACTOR * lines, &i);
    for (i = llen - 1; i >= 0; i--) {
      if (tmp[i] == '\n') {
        lines--;
        if (lines == 0) {
          i++;
          t = llen - i;
          ret = LOG_MALLOC(t);
          if (ret == NULL) {
            LOG_FREE(tmp);
            return NULL;
          }
          memcpy(ret, &tmp[i], t);
          LOG_FREE(tmp);
          *outlen = t;
          return ret;
        }
      } else if (tmp[i] < 0x0A) {
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
#endif





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
#ifdef USE_STATIC_ALLOCATION
int circularLogInit(uint8_t *buf, uint32_t bufLen) {
#else
int circularLogInit(void) {
#endif
  uint32_t res, i, si;
  LogFlashTailPtr = -1;
  LogFlashHeadPtr = -1;
#ifdef USE_STATIC_ALLOCATION
  if (bufLen < FLASH_MIN_BUFF) {
    FLASH_DEBUG("FLASH: Buffer size %u < %i\r\n", bufLen, FLASH_MIN_BUFF);
    return 0;
  }
#else
  unsigned char *buf = FLASH_MALLOC(FLASH_SECTOR_SIZE);
#endif
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
    for (i = LogFlashTailPtr; i < FLASH_LOGS_LENGTH; i += bufLen) {
      res = circFlashRead(FLASH_LOGS_ADDRESS + i, buf, bufLen);
      if (res != bufLen) {
        goto badexit;
      }
      for (si = 0; si < bufLen; si++) {
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
    for (i = 0; i < FLASH_LOGS_LENGTH; i += bufLen) {
      res = circFlashRead(FLASH_LOGS_ADDRESS + i, buf, bufLen);
      if (res != bufLen) {
        goto badexit;
      }
      for (si = 0; si < bufLen; si++) {
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
    for (i = (LogFlashHeadPtr / FLASH_SECTOR_SIZE) + 1; i < FLASH_SECTORS; i++) {
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
#ifndef USE_STATIC_ALLOCATION
  FLASH_FREE(buf);
#endif
  circLogInit = 1;
  return 1;

badexit:
  FLASH_DEBUG("FLASH: Device error\r\n");
#ifndef USE_STATIC_ALLOCATION
  FLASH_FREE(buf);
#endif
  return 0;
}
