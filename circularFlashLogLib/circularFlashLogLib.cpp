// circularFlashLogLib.cpp : Defines the entry point for the console application.
//

// ConsoleApplication20.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


#include "circularflash.h"
#define FlashLogName "flashlog.bin"

unsigned char * FakeFlash;

uint32_t circFlashRead(uint32_t FlashAddress, unsigned char * buff, uint32_t len){
	if (FlashAddress < FLASH_LOGS_ADDRESS || FlashAddress >= FLASH_LOGS_ADDRESS + FLASH_LOGS_LENGTH){
		printf("Address out of range 0x%X\r\n", FlashAddress);
		return 0;
	}
	if (FlashAddress + len > FLASH_LOGS_ADDRESS + FLASH_LOGS_LENGTH){
		printf("Address+len out of range 0x%X\r\n", FlashAddress);
		return 0;
	}
	memcpy(buff, &FakeFlash[FlashAddress - FLASH_LOGS_ADDRESS], len);
	return len;
}

uint32_t circFlashWrite(uint32_t FlashAddress, unsigned char * buff, uint32_t len){
	uint32_t i;
	if (FlashAddress < FLASH_LOGS_ADDRESS || FlashAddress >= FLASH_LOGS_ADDRESS + FLASH_LOGS_LENGTH){
		printf("Address out of range 0x%X\r\n", FlashAddress);
		return 0;
	}
	if (FlashAddress + len > FLASH_LOGS_ADDRESS + FLASH_LOGS_LENGTH){
		printf("Address+len out of range 0x%X\r\n", FlashAddress);
		return 0;
	}
	for (i = 0; i < len; i++){
		FakeFlash[FlashAddress - FLASH_LOGS_ADDRESS + i] &= buff[i];
	}
	return len;
}

uint32_t circFlashErase(uint32_t FlashAddress, uint32_t len){
	if (FlashAddress < FLASH_LOGS_ADDRESS || FlashAddress >= FLASH_LOGS_ADDRESS + FLASH_LOGS_LENGTH){
		printf("Address out of range 0x%X\r\n", FlashAddress);
		return 0;
	}
	if (FlashAddress + len > FLASH_LOGS_ADDRESS + FLASH_LOGS_LENGTH){
		printf("Address+len out of range 0x%X\r\n", FlashAddress);
		return 0;
	}
	memset(&FakeFlash[FlashAddress - FLASH_LOGS_ADDRESS], FLASH_ERASED, len);
	return len;
}


int _tmain(int argc, _TCHAR* argv[])
{
	uint32_t i;
	int32_t rem;
	FakeFlash = (unsigned char*)malloc(FLASH_LOGS_LENGTH);
	unsigned char * Read;
	FILE * FF = fopen(FlashLogName, "rb");
	if (FF != NULL){
		fread(FakeFlash, 1, FLASH_LOGS_LENGTH, FF);
		fclose(FF);
	}
	else {
		memset(FakeFlash, FLASH_ERASED, FLASH_LOGS_LENGTH);
	}

	circularLogInit();
	time_t t = time(NULL);

	uint32_t remaining, index, len;

	char * printbuf = (char*)malloc(1024);
	len = sprintf(printbuf, "New log test at %i UTC\r\n", t);
	circularWriteLog((unsigned char*)printbuf, len);
	Read = circularReadLines(1, &len);
	if (memcmp(Read, printbuf, len)){
		printf("Doesn't match\r\n");
	}
	free(Read);
	for (i = 0; i < 100000; i++){
		len = sprintf(printbuf, "Testing line %i to the log rand %i %i\r\n", i, rand(), rand());
		circularWriteLog((unsigned char*)printbuf, len);
		Read = circularReadLines(1, &len);
		if (memcmp(Read, printbuf, len)){
			printf("Line %i doesn't match, test failed\r\n");
			free(Read);
			break;
		}
		else{
			printf("\rLine %i passed:", i);
		}
		free(Read);
	}

	FF = fopen(FlashLogName, "wb");
	if (FF != NULL){
		fwrite(FakeFlash, 1, FLASH_LOGS_LENGTH, FF);
		fclose(FF);
	}
	else{
		printf("File IO error\r\n");
	}

	return 0;
}

