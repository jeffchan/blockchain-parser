#include "BlockChain.h"
#include "BitcoinTransactions.h"
#include "SHA256.h"
#include "RIPEMD160.h"
#include "BitcoinAddress.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#pragma warning(disable:4996)

#if 0 // helper method to copy a section of the blockchain to a local file
void copyFile(const char *rootDir)
{
	char scratch[512];
	sprintf(scratch,"%s\\blk%05d.dat", rootDir, 0 );	// get the filename
	FILE *fph = fopen(scratch,"rb");
	if ( fph )
	{
		size_t flen = 1000018;
		char *temp = (char *)::malloc(flen);
		fread(temp,flen,1,fph);
		fclose(fph);
		FILE *fph = fopen("blk00000.dat","wb");
		fwrite(temp,flen,1,fph);
		fclose(fph);
		::free(temp);
	}
}
#endif

int main(int argc,const char **argv)
{

	uint8_t publicKey[65] = { 0x04,0x50,0x86,0x3A,0xD6,0x4A,0x87,0xAE,0x8A,0x2F,0xE8,0x3C,0x1A,
	    					  0xF1,0xA8,0x40,0x3C,0xB5,0x3F,0x53,0xE4,0x86,0xD8,0x51,0x1D,0xAD,
	    					  0x8A,0x04,0x88,0x7E,0x5B,0x23,0x52,0x2C,0xD4,0x70,0x24,0x34,0x53,
	    					  0xA2,0x99,0xFA,0x9E,0x77,0x23,0x77,0x16,0x10,0x3A,0xBC,0x11,0xA1,
	    					  0xDF,0x38,0x85,0x5E,0xD6,0xF2,0xEE,0x18,0x7E,0x9C,0x58,0x2B,0xA6 };

	char scratch[256];
	uint8_t address1[25];
	uint8_t address2[25];
	bitcoinPublicKeyToAddress(publicKey,address1);
	bitcoinPublicKeyToAscii(publicKey,scratch,256);
	bitcoinAsciiToAddress(scratch,address2);

	const char *dataPath = "./";
	if ( argc < 2 )
	{
		printf("Using local test FILE of the first 4310 blocks in the block chain.\r\n");
	}
	else
	{
		dataPath = argv[1];
	}


	BlockChain *b = createBlockChain(dataPath);	// Create the block-chain parser using this root path
	if ( b )
	{
		const BlockChain::Block *block = b->readBlock();	// read the first block
		while ( block )
		{
			if ( block->blockIndex > 10000 ) // only process the first 171 blocks for the moment
			{
				break;
			}
			b->printBlock(block);
			block = b->readBlock(); // keep reading blocks until we are done.
		}
		b->release(); // release the block-chain parser
	}
	return 0;
}
