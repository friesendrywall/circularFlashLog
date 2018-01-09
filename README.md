# Project Title

Circular flash log library

## Getting Started

Open solution in visual studio 2013 or newer.  Compile and run demo.

## General information

This is a two file solution for circular logging text into a flash device, using
however much is desired. Its target is embedded devices. 
Configuration of flash device is in the circularFlash.h header file.  
Interaction with your flash device will happen in 3 user defined functions,
	
extern uint32_t circFlashRead(uint32_t FlashAddress, unsigned char * buff, uint32_t len);
extern uint32_t circFlashWrite(uint32_t FlashAddress, unsigned char * buff, uint32_t len);
extern uint32_t circFlashErase(uint32_t FlashAddress, uint32_t len);

## License

This project is licensed under the MIT License because other license are so annoying.

That said, please let me know of any bugs or improvements.