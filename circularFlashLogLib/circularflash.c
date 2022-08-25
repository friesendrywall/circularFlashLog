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
#include <string.h>
#include "circularFlashConfig.h"
#include "circularflash.h"
#include <stdlib.h>

extern uint32_t circFlashRead(uint32_t FlashAddress, uint8_t *buff,
                              uint32_t len);
extern uint32_t circFlashWrite(uint32_t FlashAddress, uint8_t *buff,
                               uint32_t len);
extern uint32_t circFlashErase(uint32_t FlashAddress, uint32_t len);

static int32_t calculateErasedSpace(circ_log_t * log) {
  if (log->LogFlashTailPtr == 0 && log->LogFlashHeadPtr == 0) {
    return log->logsLength; // Never written, new flash
  } else if (log->LogFlashTailPtr == -1 && log->LogFlashHeadPtr == -1) {
    FLASH_DEBUG("FLASH: (%s) Log corrupted\r\n", log->name);
    return 0;
  } else if (log->LogFlashHeadPtr > log->LogFlashTailPtr) {
    //     FlashLength     - (used space)
    return log->logsLength - (log->LogFlashHeadPtr - log->LogFlashTailPtr);
  } else if (log->LogFlashHeadPtr < log->LogFlashTailPtr) {
    //       Wrapped
    //     FlashLength     - (  Used               + (     used                             ))
    return log->logsLength - (log->LogFlashHeadPtr + (log->logsLength - log->LogFlashTailPtr));
  } else {
    FLASH_DEBUG("FLASH: (%s) Log corrupted\r\n", log->name);
    return 0;
  }
}

static int32_t calculateLogSpace(circ_log_t *log) {
  if (log->LogFlashTailPtr == 0 && log->LogFlashHeadPtr == 0) {
    return 0; // Never written, new flash
  } else if (log->LogFlashTailPtr == -1 && log->LogFlashHeadPtr == -1) {
    FLASH_DEBUG("FLASH: (%s) Log corrupted\r\n", log->name);
    return 0;
  } else if (log->LogFlashHeadPtr > log->LogFlashTailPtr) {
    return (log->LogFlashHeadPtr - log->LogFlashTailPtr);
  } else if (log->LogFlashHeadPtr < log->LogFlashTailPtr) {
    return log->LogFlashHeadPtr + (log->logsLength - log->LogFlashTailPtr);
  } else {
    FLASH_DEBUG("FLASH: (%s) Log corrupted\r\n", log->name);
    return 0;
  }
}

/* This function inserts, assuming that writing 1's won't change anything*/
static uint32_t circFlashInsertWrite(circ_log_t *log, uint32_t FlashAddress,
                                     unsigned char *buff, uint32_t len) {
  uint32_t i, rem, end, begin, WriteLen, res;
  rem = FlashAddress % FLASH_WRITE_SIZE;
  begin = FlashAddress - rem;
  end = FlashAddress + len; // Extend up to boundary
  if (end % FLASH_WRITE_SIZE) {
    WriteLen = (((end - begin) / FLASH_WRITE_SIZE) + 1) * FLASH_WRITE_SIZE;
  } else {
    WriteLen = (((end - begin) / FLASH_WRITE_SIZE)) * FLASH_WRITE_SIZE;
  }
  if (WriteLen > log->wBuffLen) {
    uint32_t startLen = len;
    if (rem) {
      memset(log->wBuff, FLASH_ERASED, FLASH_WRITE_SIZE);
      memcpy(&log->wBuff[rem], buff, FLASH_WRITE_SIZE - rem);
      res = circFlashWrite(begin, log->wBuff, FLASH_WRITE_SIZE);
      if (res != FLASH_WRITE_SIZE) {
        return 0;
      }
      len -= (FLASH_WRITE_SIZE - rem);
      buff += (FLASH_WRITE_SIZE - rem);
      begin += FLASH_WRITE_SIZE;
      WriteLen -= FLASH_WRITE_SIZE;
    }
    // Send the rest
    for (i = 0; i < WriteLen; i += FLASH_WRITE_SIZE) {
      memset(log->wBuff, FLASH_ERASED, FLASH_WRITE_SIZE);
      memcpy(log->wBuff, &buff[i],
             len > FLASH_WRITE_SIZE ? FLASH_WRITE_SIZE : len);
      res = circFlashWrite(begin + i, log->wBuff, FLASH_WRITE_SIZE);
      if (res != FLASH_WRITE_SIZE) {
        return 0;
      }
      len -= FLASH_WRITE_SIZE;
    }
    return startLen;
  } else {
    memset(log->wBuff, FLASH_ERASED, WriteLen);
    memcpy(&log->wBuff[rem], buff, len);
    res = circFlashWrite(begin, log->wBuff, WriteLen);
    return res == WriteLen ? len : 0;
  }
}

/**
* seek = Bytes from start of log
 */
uint32_t circularReadLogPartial(circ_log_t *log, uint8_t *buff,
                               uint32_t seek, uint32_t desiredlen,
                               uint32_t *remaining) {
  uint32_t ret = 0;
  uint32_t res, firstlen, secondlen;
  if (!log->circLogInit) {
    return 0;
  }
  FLASH_MUTEX_ENTER(log->osMutex);
  int space = calculateLogSpace(log);
  if (desiredlen > (space - seek)) {
    desiredlen = (space - seek);
  }
  if (space > 0 && desiredlen > 0) {
    if (log->LogFlashHeadPtr > log->LogFlashTailPtr) {
      res = circFlashRead(log->baseAddress + log->LogFlashTailPtr + seek, buff,
                          desiredlen);
      if (res != desiredlen) {
        FLASH_DEBUG("FLASH: (%s) IO error\r\n", log->name);
        ret = 0;
        *remaining = 0;
        goto badexit;
      }
      *remaining = (space - seek - desiredlen);
      ret = desiredlen;
    } else if (log->LogFlashHeadPtr < log->LogFlashTailPtr) {
      // See which part the caller wants
      firstlen = log->logsLength - log->LogFlashTailPtr;
      if (seek > firstlen) {
        // The upper half of the request
        res = circFlashRead(log->baseAddress + (seek - firstlen), buff,
                            desiredlen);
        if (res != desiredlen) {
          FLASH_DEBUG("FLASH: (%s) IO error\r\n", log->name);
          ret = 0;
          *remaining = 0;
          goto badexit;
        }
        ret = desiredlen;
        *remaining = (space - seek - desiredlen);
      } else {
        // Lower half first from end of address space
        if (seek + desiredlen + log->LogFlashTailPtr > log->logsLength) {
          secondlen = log->logsLength - (log->LogFlashTailPtr + seek);
          res = circFlashRead(log->baseAddress + log->LogFlashTailPtr + seek, buff,
                              secondlen);
          if (res != secondlen) {
            FLASH_DEBUG("FLASH: (%s) IO error\r\n", log->name);
            ret = 0;
            *remaining = 0;
            goto badexit;
          }
          res = circFlashRead(log->baseAddress, &buff[secondlen],
                              desiredlen - secondlen);
          if (res != desiredlen - secondlen) {
            FLASH_DEBUG("FLASH: (%s) IO error\r\n", log->name);
            ret = 0;
            *remaining = 0;
            goto badexit;
          }
          ret = desiredlen;
          *remaining = (space - seek - desiredlen);
        } else {
          res = circFlashRead(log->baseAddress + log->LogFlashTailPtr + seek, buff,
                              desiredlen);
          if (res != desiredlen) {
            FLASH_DEBUG("FLASH: (%s) IO error\r\n", log->name);
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
  FLASH_MUTEX_EXIT(log->osMutex);
  return ret;
}

#ifdef USE_STATIC_ALLOCATION

uint32_t circularReadLines(circ_log_t *log, uint8_t *buff, uint32_t buffSize,
                           uint32_t lines, char *filter) {
  uint32_t ret = 0;
  uint32_t remaining;
  int32_t space, seek, i;
  uint32_t llen, searchLen;
  uint32_t lastStart = 0;
  if (buffSize < LINE_ESTIMATE_FACTOR) {
    return 0;
  }
  FLASH_MUTEX_ENTER(log->osMutex);
  space = calculateLogSpace(log);
  FLASH_MUTEX_EXIT(log->osMutex);
  searchLen = lines * LINE_ESTIMATE_FACTOR;
  if (searchLen > buffSize - 1) {
    searchLen = buffSize - 1;
  }
  seek = space - searchLen;
  if (seek < 0) {
    seek = 0;
  }

  ret = circularReadLogPartial(log, buff, seek, searchLen, &remaining);
  if (ret == 0) {
    return 0;
  }

  // Search for "\n"
  for (i = ret - 3; i >= 0; i--) {
    if (buff[i] == '\n') {
      lines--;
      lastStart = i + 1;
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
    for (i = 0; i < (int32_t)ret; i++) {
      if (buff[i] == '\n') {
        buff[i] = 0;
        if (strstr((char *)LastLine, filter) != NULL) {
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
      snprintf((char *)buff, buffSize,
          "** Search item '%s' not found in %i lines **\r\n", filter,
          lines);
      ret = (uint32_t)strlen((char *)buff);
    } else {
      ret = FoundLength;
      buff[FoundLength] = 0;
    }
  } else {
    if (lines) {
      // Finalize it
      ret -= lastStart;
      memcpy(buff, &buff[lastStart], ret);
      buff[ret] = 0;
    }
  }
  return ret;
}

#else
/*
 * Returns a LOG_MALLOC'ed pointer to the log
 */
uint8_t *circularReadLog(circ_log_t *log, uint32_t *len) {
  uint8_t *ret = NULL;
  uint32_t res, firstlen;
  static char NoLogs[] = "Empty log";
  if (!log->circLogInit) {
    *len = 0;
    return NULL;
  }
  FLASH_MUTEX_ENTER(log->osMutex);
  int space = calculateLogSpace(log);
  if (space > 0) {
    ret = LOG_MALLOC(space + 1);
    if (ret == NULL) {
      FLASH_DEBUG("FLASH: Malloc error\r\n");
      goto badexit;
    }
    if (log->LogFlashHeadPtr > log->LogFlashTailPtr) {
#ifdef LOG_CACHE_INVALIDATE
      LOG_CACHE_INVALIDATE(ret, space + 1);
#endif
      res = circFlashRead(log->baseAddress + log->LogFlashTailPtr, ret, space);
      *len = res + 1;
      if (res != space) {
        FLASH_DEBUG("FLASH: (%s) IO error\r\n", log->name);
      }
      ret[res] = 0;
    } else if (log->LogFlashHeadPtr < log->LogFlashTailPtr) {
      firstlen = log->logsLength - log->LogFlashTailPtr;
#ifdef LOG_CACHE_INVALIDATE
      LOG_CACHE_INVALIDATE(ret, space + 1);
#endif
      res = circFlashRead(log->baseAddress + log->LogFlashTailPtr, ret, firstlen);
      if (res != firstlen) {
        FLASH_DEBUG("FLASH: (%s) IO error\r\n", log->name);
      }
      res = circFlashRead(log->baseAddress, &ret[firstlen], log->LogFlashHeadPtr);
      if (res != log->LogFlashHeadPtr) {
        FLASH_DEBUG("FLASH: (%s) IO error\r\n", log->name);
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
  FLASH_MUTEX_EXIT(log->osMutex);
  return ret;
}

/*
 * Returns a LOG_MALLOC'ed pointer to the number of log lines requested
 */
uint8_t *circularReadLines(circ_log_t *log, uint32_t lines,
                                 uint32_t *outlen) {
  uint32_t llen = 0;
  int32_t i, seek;
  uint32_t t;
  uint8_t *ret;
  if (!lines) {
    *outlen = 0;
    return 0;
  }
  lines++;
  if (!log->circLogInit) {
    *outlen = 0;
    return NULL;
  }
  unsigned char *tmp = LOG_MALLOC(LINE_ESTIMATE_FACTOR * lines);
  if (tmp) {
    FLASH_MUTEX_ENTER(log->osMutex);
    seek = calculateLogSpace(log);
    seek -= LINE_ESTIMATE_FACTOR * lines;
    if (seek < 0) {
      seek = 0;
    }
    FLASH_MUTEX_EXIT(log->osMutex);
#ifdef LOG_CACHE_INVALIDATE
    LOG_CACHE_INVALIDATE(tmp, LINE_ESTIMATE_FACTOR * lines);
#endif
    llen = circularReadLogPartial(log, tmp, seek, LINE_ESTIMATE_FACTOR * lines, &i);
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

uint32_t circularClearLog(circ_log_t *log) {
  FLASH_MUTEX_ENTER(log->osMutex);
  if (circFlashErase(log->baseAddress, log->logsLength) !=
      log->logsLength) {
    FLASH_DEBUG("FLASH: (%s) Erase IO error\r\n", log->name);
    goto badexit;
  }
  FLASH_DEBUG("FLASH: (%s) Entire flash erased\r\n", log->name);
  log->LogFlashTailPtr = log->LogFlashHeadPtr = 0;
  FLASH_MUTEX_EXIT(log->osMutex);
  return CIRC_LOG_ERR_NONE;
badexit:
  FLASH_MUTEX_EXIT(log->osMutex);
  return CIRC_LOG_ERR_IO;
}

/*
 *
 */
uint32_t circularWriteLog(circ_log_t *log, uint8_t *buf, uint32_t len) {
  int32_t EraseSpace;
  uint32_t res, firstlen;
  if (len > FLASH_SECTOR_SIZE) {
    len = FLASH_SECTOR_SIZE;
  }
  FLASH_MUTEX_ENTER(log->osMutex);
  EraseSpace = calculateErasedSpace(log);
  if (EraseSpace == 0) {
    // Erase it all
    if (circFlashErase(log->baseAddress, log->logsLength) != log->logsLength) {
      FLASH_DEBUG("FLASH: (%s) Erase IO error\r\n", log->name);
      goto badexit;
    }
    FLASH_DEBUG("FLASH: (%s) Entire flash erased\r\n", log->name);
    log->LogFlashTailPtr = log->LogFlashHeadPtr = 0;
  } else if (EraseSpace < (FLASH_SECTOR_SIZE * 2)) {
    // Erase next sector in line
    if (circFlashErase(log->baseAddress + log->LogFlashTailPtr,
                       FLASH_SECTOR_SIZE) != FLASH_SECTOR_SIZE) {
      FLASH_DEBUG("FLASH: (%s) Erase IO error\r\n", log->name);
      goto badexit;
    }
    FLASH_DEBUG("FLASH: (%s) Sector at address 0x%X erased\r\n", log->name,
                log->baseAddress + log->LogFlashTailPtr);
    log->LogFlashTailPtr += FLASH_SECTOR_SIZE;
    if (log->LogFlashTailPtr >= (int32_t)log->logsLength) {
      log->LogFlashTailPtr = 0;
    }
  }
  // Does it wrap?
  // The FLASH_WRITE_SIZE boundary will assume that writing FLASH_ERASED will
  // leave existing alone
  if (log->LogFlashHeadPtr + len > log->logsLength) {
    // Wrapped
    firstlen = log->logsLength - log->LogFlashHeadPtr;
    res = circFlashInsertWrite(log, log->baseAddress + log->LogFlashHeadPtr, buf,
                               firstlen);
    if (res != firstlen) {
      FLASH_DEBUG("FLASH: (%s) Write IO error\r\n", log->name);
      goto badexit;
    }
    res = circFlashInsertWrite(log, log->baseAddress, &buf[firstlen],
                               len - firstlen);
    if (res != len - firstlen) {
      FLASH_DEBUG("FLASH: (%s) Write IO error\r\n", log->name);
      goto badexit;
    }
    log->LogFlashHeadPtr = len - firstlen;
  } else {
    res = circFlashInsertWrite(log, log->baseAddress + log->LogFlashHeadPtr,
                               buf, len);
    if (res != len) {
      FLASH_DEBUG("FLASH: (%s) Write IO error\r\n", log->name);
      goto badexit;
    }
    log->LogFlashHeadPtr += len;
  }
  FLASH_MUTEX_EXIT(log->osMutex);
  return len;
badexit:
  FLASH_MUTEX_EXIT(log->osMutex);
  return 0;
}

uint32_t circularLogInit(circ_log_t *log) {
  uint32_t res, i, si;
  log->LogFlashTailPtr = -1;
  log->LogFlashHeadPtr = -1;
#ifdef USE_STATIC_ALLOCATION
  if (log->wBuffLen < FLASH_MIN_BUFF) {
    FLASH_DEBUG("FLASH: (%s) Buffer size %u < %i\r\n", log->name, log->wBuffLen,
                FLASH_MIN_BUFF);
    return CIRC_LOG_ERR_API;
  }
  uint32_t bufLen = log->wBuffLen;
  uint8_t *buf = log->wBuff;
#else
  uint8_t *buf = FLASH_MALLOC(FLASH_SECTOR_SIZE);
  uint32_t bufLen = FLASH_SECTOR_SIZE;
#endif
  if (buf == NULL) {
    return CIRC_LOG_ERR_ALLOC;
  }
  res = circFlashRead(log->baseAddress, buf, 4);
  if (res != 4) {
    goto badexit;
  }

  if (buf[0] == FLASH_ERASED) {
    // Search for tail first
    for (i = 1; i < FLASH_SECTORS(log->logsLength); i++) {
      res = circFlashRead(log->baseAddress + (FLASH_SECTOR_SIZE * i), buf, 4);
      if (res != 4) {
        goto badexit;
      }

      if (buf[0] != FLASH_ERASED) {
        log->LogFlashTailPtr = i * FLASH_SECTOR_SIZE;
        break;
      }
    }
    if (log->LogFlashTailPtr == -1) {
      // Device is empty
      FLASH_DEBUG("FLASH: (%s) Device is empty\r\n", log->name);
      log->LogFlashTailPtr = 0;
      log->LogFlashHeadPtr = 0;
      goto goodexit;
    }
    // Now search for head
    for (i = log->LogFlashTailPtr; i < log->logsLength; i += bufLen) {
      res = circFlashRead(log->baseAddress + i, buf, bufLen);
      if (res != bufLen) {
        goto badexit;
      }
      for (si = 0; si < bufLen; si++) {
        if (buf[si] == FLASH_ERASED) {
          log->LogFlashHeadPtr = i + si;
          break;
        }
      }
      if (log->LogFlashHeadPtr != -1) {
        break;
      }
    }
    if (log->LogFlashHeadPtr == -1) {
      // This would only happen if head is at 0
      log->LogFlashHeadPtr = 0;
    }

  } else {
    // Search for head first
    for (i = 0; i < log->logsLength; i += bufLen) {
      res = circFlashRead(log->baseAddress + i, buf, bufLen);
      if (res != bufLen) {
        goto badexit;
      }
      for (si = 0; si < bufLen; si++) {
        if (buf[si] == FLASH_ERASED) {
          log->LogFlashHeadPtr = i + si;
          break;
        }
      }
      if (log->LogFlashHeadPtr != -1) {
        break;
      }
    }

    if (log->LogFlashHeadPtr == -1) {
      // Device is full
      FLASH_DEBUG("FLASH: (%s) Device is full\r\n", log->name);
      log->LogFlashTailPtr = -1;
      log->LogFlashHeadPtr = -1;
      goto goodexit;
    }

    // Now search for tail
    for (i = (log->LogFlashHeadPtr / FLASH_SECTOR_SIZE) + 1;
         i < FLASH_SECTORS(log->logsLength); i++) {
      res = circFlashRead(log->baseAddress + (FLASH_SECTOR_SIZE * i), buf, 4);
      if (res != 4) {
        goto badexit;
      }
      if (buf[0] != FLASH_ERASED) {
        log->LogFlashTailPtr = i * FLASH_SECTOR_SIZE;
        break;
      }
    }
    if (log->LogFlashTailPtr == -1) {
      // This would only happen if tail is 0
      log->LogFlashTailPtr = 0;
    }
  }
goodexit:
  FLASH_DEBUG("FLASH: (%s) Head is at 0x%X\r\n", log->name,
              log->LogFlashHeadPtr);
  FLASH_DEBUG("FLASH: (%s) Tail is at 0x%X\r\n", log->name,
              log->LogFlashTailPtr);
  FLASH_DEBUG("FLASH: (%s) Erased --  0x%X\r\n", log->name,
              calculateErasedSpace(log));
  FLASH_DEBUG("FLASH: (%s) Logs ----  0x%X\r\n", log->name,
              calculateLogSpace(log));
#ifndef USE_STATIC_ALLOCATION
  FLASH_FREE(buf);
#endif
  log->circLogInit = 1;
  return CIRC_LOG_ERR_NONE;

badexit:
  FLASH_DEBUG("FLASH: (%s) Device error\r\n", log->name);
#ifndef USE_STATIC_ALLOCATION
  FLASH_FREE(buf);
#endif
  return CIRC_LOG_ERR_IO;
}
