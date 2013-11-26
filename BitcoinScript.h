#ifndef SCRIPT_H

#define SCRIPT_H

// Work in progress on a Bitcoin script virtual machine; not finished.

#include <stdint.h>	// Include stdint.h; available on most compilers but, if not, a copy is provided here for Microsoft Visual Studio

class BitcoinScript
{
public:
	virtual const char *getLastError(void) const = 0;
	virtual void resetStack(void) = 0;
	virtual bool executeScript(const uint8_t *script,uint32_t scriptLength) = 0;
	virtual void release(void) = 0;
};

BitcoinScript *createBitcoinScript(void);

#endif
