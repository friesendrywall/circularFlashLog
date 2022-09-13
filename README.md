# Project Title

Circular flash log library

## Getting Started

Open solution in visual studio 2019 or newer.  Compile and run demo.

## General information and setup

This is a two file solution for circular logging text into a serial flash device, using
however much flash may be desired. Its target is embedded devices. 
Configuration of flash device is in the circularFlash.h header file.  
Interaction with your flash device will happen in 3 user defined functions,
	
extern uint32_t circFlashRead(uint32_t FlashAddress, unsigned char * buff, uint32_t len);
extern uint32_t circFlashWrite(uint32_t FlashAddress, unsigned char * buff, uint32_t len);
extern uint32_t circFlashErase(uint32_t FlashAddress, uint32_t len);

In circularFlashConfig.h, define the core flash information
```
#define FLASH_SECTOR_SIZE	0x1000
#define FLASH_WRITE_SIZE	0x100
```

In the application, set up the log device
```
/* Flash start address */
#define FLASH_LOGS_ADDRESS  0x200000
/* Flash length */
#define FLASH_LOGS_LENGTH   0x1E0000UL

  uint8_t workingBuff[FLASH_WRITE_SIZE * 2];
  circ_log_t log = {.name = "LOGS",
                    .baseAddress = FLASH_LOGS_ADDRESS,
                    .logsLength = FLASH_LOGS_LENGTH,
                    .wBuff = workingBuff,
                    .wBuffLen = sizeof(workingBuff)};
```

This library manages flash limitations by writing small portions into pages and surrounding them with FLASH_ERASED. This allows small incremental additions smaller than FLASH_WRITE_SIZE to the flash device.

## License

This project is licensed under the MIT License

Please let me know of any bugs or improvements.
