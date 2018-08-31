# Project Title

Circular flash log library

## Getting Started

Open solution in visual studio 2013 or newer.  Compile and run demo.

## General information

This is a two file solution for circular logging text into a serial flash device, using
however much flash may be desired. Its target is embedded devices. 
Configuration of flash device is in the circularFlash.h header file.  
Interaction with your flash device will happen in 3 user defined functions,
	
extern uint32_t circFlashRead(uint32_t FlashAddress, unsigned char * buff, uint32_t len);
extern uint32_t circFlashWrite(uint32_t FlashAddress, unsigned char * buff, uint32_t len);
extern uint32_t circFlashErase(uint32_t FlashAddress, uint32_t len);

In circularflash.h, the location of flash is defined as

#define FLASH_LOGS_ADDRESS  0x200000
#define FLASH_LOGS_LENGTH   0x1E0000UL
#define FLASH_SECTOR_SIZE	0x1000
#define FLASH_WRITE_SIZE	0x100
#define FLASH_ERASED (0xFF)

FLASH_LOGS_ADDRESS defines the log start location
FLASH_LOGS_LENGTH  defines the length
FLASH_SECTOR_SIZE  defines the erase sector size
FLASH_WRITE_SIZE   defines the minimum flash write size
FLASH_ERASED       defines the erased state of flash

This library manages flash limitations by writing small portions into pages and surrounding them with FLASH_ERASED. This allows small incremental additions smaller than FLASH_WRITE_SIZE to the flash device.

## License

This project is licensed under the MIT License because other license are so annoying.

That said, please let me know of any bugs or improvements.
