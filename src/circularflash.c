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
      res = log->write(begin, log->wBuff, FLASH_WRITE_SIZE);
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
      res = log->write(begin + i, log->wBuff, FLASH_WRITE_SIZE);
      if (res != FLASH_WRITE_SIZE) {
        return 0;
      }
      len -= FLASH_WRITE_SIZE;
    }
    return startLen;
  } else {
    memset(log->wBuff, FLASH_ERASED, WriteLen);
    memcpy(&log->wBuff[rem], buff, len);
    res = log->write(begin, log->wBuff, WriteLen);
    return res == WriteLen ? len : 0;
  }
}

/**
* seek = Bytes from start of log
 */
uint32_t circularReadLogPartial(circ_log_t *log, uint8_t *buff,
                               uint32_t seek, uint32_t desiredlen,
                               uint32_t *remaining) {
  CIRCULAR_LOG_ASSERT(log != NULL);
  CIRCULAR_LOG_ASSERT(buff != NULL);
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
      res = log->read(log->baseAddress + log->LogFlashTailPtr + seek, buff,
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
        res = log->read(log->baseAddress + (seek - firstlen), buff,
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
          res = log->read(log->baseAddress + log->LogFlashTailPtr + seek, buff,
                              secondlen);
          if (res != secondlen) {
            FLASH_DEBUG("FLASH: (%s) IO error\r\n", log->name);
            ret = 0;
            *remaining = 0;
            goto badexit;
          }
          res = log->read(log->baseAddress, &buff[secondlen],
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
          res = log->read(log->baseAddress + log->LogFlashTailPtr + seek, buff,
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

uint32_t circularReadLines(circ_log_t *log, uint8_t *buff, uint32_t buffSize,
                           uint32_t lines, char *filter,
                           uint32_t estLineLength) {
  uint32_t ret = 0;
  uint32_t remaining;
  int32_t space, seek, i;
  uint32_t llen, searchLen;
  uint32_t lastStart = 0;
  CIRCULAR_LOG_ASSERT(log != NULL);
  CIRCULAR_LOG_ASSERT(buff != NULL);
  if (estLineLength == 0) {
    estLineLength = LINE_ESTIMATE_FACTOR;
  }
  if (buffSize < estLineLength) {
    return 0;
  }
  FLASH_MUTEX_ENTER(log->osMutex);
  space = calculateLogSpace(log);
  FLASH_MUTEX_EXIT(log->osMutex);
  searchLen = lines * estLineLength;
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

uint32_t circularClearLog(circ_log_t *log) {
  CIRCULAR_LOG_ASSERT(log != NULL);
  FLASH_MUTEX_ENTER(log->osMutex);
  if (log->erase(log->baseAddress, log->logsLength) !=
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
  CIRCULAR_LOG_ASSERT(log != NULL);
  CIRCULAR_LOG_ASSERT(buf != NULL);
  if (len > FLASH_SECTOR_SIZE) {
    len = FLASH_SECTOR_SIZE;
  }
  FLASH_MUTEX_ENTER(log->osMutex);
  EraseSpace = calculateErasedSpace(log);
  if (EraseSpace == 0) {
    // Erase it all
    if (log->erase(log->baseAddress, log->logsLength) != log->logsLength) {
      FLASH_DEBUG("FLASH: (%s) Erase IO error\r\n", log->name);
      goto badexit;
    }
    FLASH_DEBUG("FLASH: (%s) Entire flash erased\r\n", log->name);
    log->LogFlashTailPtr = log->LogFlashHeadPtr = 0;
  } else if (EraseSpace < (FLASH_SECTOR_SIZE * 2)) {
    // Erase next sector in line
    if (log->erase(log->baseAddress + log->LogFlashTailPtr,
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
  CIRCULAR_LOG_ASSERT(log != NULL);
  CIRCULAR_LOG_ASSERT(log->wBuff != NULL);
  CIRCULAR_LOG_ASSERT(log->read);
  CIRCULAR_LOG_ASSERT(log->write);
  CIRCULAR_LOG_ASSERT(log->erase);
  FLASH_MUTEX_ENTER(log->osMutex);
  log->LogFlashTailPtr = -1;
  log->LogFlashHeadPtr = -1;
  if (log->wBuffLen < FLASH_MIN_BUFF) {
    FLASH_DEBUG("FLASH: (%s) Buffer size %u < %i\r\n", log->name, log->wBuffLen,
                FLASH_MIN_BUFF);
    return CIRC_LOG_ERR_API;
  }
  uint32_t bufLen = log->wBuffLen;
  uint8_t *buf = log->wBuff;
  res = log->read(log->baseAddress, buf, 4);
  if (res != 4) {
    goto badexit;
  }

  if (buf[0] == FLASH_ERASED) {
    // Search for tail first
    for (i = 1; i < FLASH_SECTORS(log->logsLength); i++) {
      res = log->read(log->baseAddress + (FLASH_SECTOR_SIZE * i), buf, 4);
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
      res = log->read(log->baseAddress + i, buf, bufLen);
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
      res = log->read(log->baseAddress + i, buf, bufLen);
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
      res = log->read(log->baseAddress + (FLASH_SECTOR_SIZE * i), buf, 4);
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
  FLASH_DEBUG("FLASH: (%s) 0x%X .. 0x%X .. 0x%X\r\n", log->name, log->LogFlashTailPtr,
      log->LogFlashHeadPtr, calculateErasedSpace(log));
  log->circLogInit = 1;
  FLASH_MUTEX_EXIT(log->osMutex);
  return CIRC_LOG_ERR_NONE;

badexit:
  FLASH_MUTEX_EXIT(log->osMutex);
  FLASH_DEBUG("FLASH: (%s) Device error\r\n", log->name);
  return CIRC_LOG_ERR_IO;
}