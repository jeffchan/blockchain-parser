#include "BlockChain.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <float.h>
#include <math.h>
#include <time.h>

#include "keyboard.h"
#include "BlockChainAddresses.h"

#pragma warning(disable:4996 4702 4505)

time_t zombieDate(0x510B56CB); // right around January 1, 2013

static const char *getTimeString(uint32_t timeStamp)
{
	static char scratch[1024];
	time_t t(timeStamp);
	struct tm *gtm = gmtime(&t);
	strftime(scratch, 1024, "%m/%d/%Y %H:%M:%S", gtm);
	return scratch;
}

enum StatResolution
{
	SR_DAY,
	SR_MONTH,
	SR_YEAR,
	SR_LAST
};

enum CommandMode
{
	CM_NONE,	//
	CM_SCAN,	// scanning.
	CM_PROCESS,
	CM_EXIT
};

class BlockChainCommand
{
public:
	BlockChainCommand(const char *dataPath)
	{
		mBlockChain = createBlockChain(dataPath);	// Create the block-chain parser using this root path
		mStatResolution = SR_YEAR;
		mMaxBlock = 500000;
		printf("Welcome to the BlockChain command parser.  Written by John W. Ratcliff on January 4, 2014 : TipJar: 1BT66EoaGySkbY9J6MugvQRhMMXDwPxPya\r\n");
		printf("Registered DataDirectory: %s to scan for the blockchain.\r\n", dataPath );
		printf("\r\n");
		mProcessTransactions = false;
		mProcessBlock = 0;
		mLastBlockScan = 0;
		mLastBlockPrint = 0;
		mFinishedScanning = false;
		mCurrentBlock = NULL;
		mLastTime = 0;
		mSatoshiTime = 0;
		mMinBalance = 1;
		mRecordAddresses = false;
		mAddresses = NULL;
		mMode = CM_NONE;

		if ( mBlockChain )
		{
			help();
		}
		else
		{
			printf("Failed to open file: blk00000.dat in directory: %s\r\n", dataPath );
			mMode = CM_EXIT;
		}
	}

	~BlockChainCommand(void)
	{
		if ( mAddresses )
		{
			mAddresses->release();
		}
		if ( mBlockChain )
		{
			mBlockChain->release();
		}
	}

	void help(void)
	{
		printf("Commands Available:\r\n");
		printf("\r\n");

		printf("max_blocks            : Specifies the maximum number of blocks to read.\r\n");
		printf("scan                  : Toggles scanning the blockchain headers pressing a key will pause or abort the scan.\r\n");
		printf("process               : Toggle processing all blocks; warning uses a lot of memory!..\r\n");
		printf("statistics            : Enables gathering detailed address/transaction statistics on the block chain\r\n");
		printf("\r\n");
		printf("stop_scan             : Stop's the scan of the blockchain headers and just builds the blockchain from where we are at so far.\r\n");
		printf("block <number>        : Will print the contents of this block.\r\n");
		printf("counts                : Report block and transaction counts.\r\n");
		printf("by_day                : Reports statistics by day.\r\n");
		printf("by_month              : Reports statistics by month.\r\n");
		printf("by_year               : Reports statistics by year.\r\n");
		printf("top_balance <n>       : Will output the addresses with the highest balance up to <n>\r\n");
		printf("min_balance <n>       : Specifies the minimum balance to use when generating a report. Default is 1BTC\r\n");
		printf("oldest <n>            : Outputs the <n> oldest addresses higher than min_balance\r\n");
		printf("adr <n>               : Outputs the transaction history relative to a specific bitcoinaddres.\r\n");
		printf("zombie <days>         : Report statitics about zombie coins; contents of addresses not used since this many days.\r\n");
		printf("record_addresses      : Toggles whether or not to record addresses when computing statistics.\r\n");
		printf("load_record           : Debugging feature, tries to load previously recorded addresses.\r\n");
		printf("help                  : Repeat these commands.\r\n");
		printf("exit, quit, or bye    : Will exit this tool\r\n");
	}

	void stopScanning(void)
	{
		mFinishedScanning = true;
		mMode = CM_NONE; // done scanning.
		mLastBlockScan = mBlockChain->buildBlockChain();
		mCurrentBlock = mBlockChain->readBlock(0);
		printf("Stopped scanning block headers early. Built block-chain with %d blocks found..\r\n", mLastBlockScan);
	}

	bool process(void)
	{
		uint32_t argc;
		const char **argv = getInputString(argc);

		if ( argv )
		{
			if ( strcmp(argv[0],"scan") == 0 )
			{
				if ( mMode == CM_SCAN )
				{
					printf("Pausing block-chain scan at block #%d\r\n", mLastBlockScan );
					mMode = CM_NONE;
				}
				else
				{
					printf("Scanning block-chain re-started from block %d up to a maximum of %d blocks..\r\n", mLastBlockScan, mMaxBlock);
					mMode = CM_SCAN;
				}
			}
			else if ( strcmp(argv[0],"exit") == 0 ||  strcmp(argv[0],"bye") == 0 || strcmp(argv[0],"quit") == 0  )
			{
				mMode = CM_EXIT;
			}
			else if ( strcmp(argv[0],"stop_scan") == 0 )
			{
				if ( mFinishedScanning )
				{
					printf("Already finished scanning the block-chain headers.  Found %d blocks.\r\n", mLastBlockScan );
				}
				else
				{
					stopScanning();
				}
			}
			else if ( strcmp(argv[0],"help") == 0 )
			{
				help();
			}
			else if ( strcmp(argv[0],"max_blocks") == 0 )
			{
				if ( argc >= 2 )
				{
					mMaxBlock = atoi(argv[1]);
					if ( mMaxBlock < 1 ) mMaxBlock = 1;
					printf("Maximum block scan set to %d\r\n", mMaxBlock );
				}
			}
			else if ( strcmp(argv[0],"by_day") == 0 )
			{
				mStatResolution = SR_DAY;
				printf("Will accumulate statistics on a per-day basis.\r\n");
				if ( !mProcessTransactions )
				{
					printf("Note: You must enable 'statistics' for this to take effect.\r\n");
				}
			}
			else if ( strcmp(argv[0],"by_month") == 0 )
			{
				mStatResolution = SR_MONTH;
				printf("Will accumulate statistics on a monthly basis.\r\n");
				if ( !mProcessTransactions )
				{
					printf("Note: You must enable 'statistics' for this to take effect.\r\n");
				}
			}
			else if ( strcmp(argv[0],"by_year") == 0 )
			{
				mStatResolution = SR_YEAR;
				printf("Will accumulate statistics on an annual basis.\r\n");
				if ( !mProcessTransactions )
				{
					printf("Note: You must enable 'statistics' for this to take effect.\r\n");
				}
			}
			else if ( strcmp(argv[0],"record_addresses") == 0 )
			{
				mRecordAddresses = mRecordAddresses ? false : true;
				printf("record_addresses set to %s\r\n", mRecordAddresses ? "true" : "false");
			}
			else if ( strcmp(argv[0],"adr") == 0 )
			{
				if ( argc == 1 )
				{
					printf("You must supply an address to output.\r\n");
				}
				else
				{
					for (uint32_t i=1; i<argc; i++)
					{
						const char *adr = argv[i];
						mBlockChain->printAddress(adr);
					}
				}
			}
			else if ( strcmp(argv[0],"process") == 0 )
			{
				if ( mMode == CM_PROCESS )
				{
					printf("Pausing processing block-chain blocks at block #%d of %d\r\n", mProcessBlock, mBlockChain->getBlockCount() );
					mMode = CM_NONE;
				}
				else
				{
					if ( !mFinishedScanning )
					{
						stopScanning();
					}
					mProcessBlock = 0;
					mMode = CM_PROCESS;
					printf("Beginning processing of %d blocks : Gathering Statistics=%s\r\n", 
					mBlockChain->getBlockCount(), 
					mProcessTransactions ? "true":"false");
				}
			}
			else if ( strcmp(argv[0],"statistics") == 0 )
			{
				mProcessTransactions = mProcessTransactions ? false : true;
				if ( mProcessTransactions )
				{
					printf("Block Processing will gather statistics.\r\n");
					printf("*** WARNING : This will consume an enormous amount of memory! ***\r\n");
				}
				else
				{
					printf("Block processing will not gather statistics.\r\n");
				}
			}
			else if ( strcmp(argv[0],"load_record") == 0 )
			{
				if ( mAddresses )
				{
					mAddresses->release();
					mAddresses = NULL;
				}
				printf("Loading previously recorded addresses from file 'BlockChainAddresses.bin\r\n");
				mAddresses = createBlockChainAddresses("BlockChainAddresses.bin");
			}
			else if ( strcmp(argv[0],"min_balance") == 0 )
			{
				if ( argc == 1 )
				{
					printf("No minimum balance specified, defaulting to 1btc.\r\n");
				}
				else
				{
					mMinBalance = (uint32_t)atoi(argv[1]);
					if ( mMinBalance < 1 )
					{
						mMinBalance = 1;
					}
					else
					{
						if ( mMinBalance > 1000000 )
						{
							mMinBalance = 1000000;
						}
					}
					printf("Minimum balance set to %d bitcoins.\r\n", mMinBalance );
				}
			}
			else if ( strcmp(argv[0],"top_balance") == 0 )
			{
				uint32_t tcount = 100;
				if ( argc >= 2 )
				{
					tcount = atoi(argv[1]);
					if ( tcount < 1 )
					{
						tcount = 1;
					}
				}
				printf("Printing the most valuable %d bitcoin addresses with a balance of more than %d bitcoins.\r\n", tcount, mMinBalance );
				mBlockChain->printTopBalances(tcount,mMinBalance);
			}
			else if ( strcmp(argv[0],"oldest") == 0 )
			{
				uint32_t tcount = 100;
				if ( argc >= 2 )
				{
					tcount = atoi(argv[1]);
					if ( tcount < 1 )
					{
						tcount = 1;
					}
				}
				printf("Printing the %d oldest bitcoin addresses with a balance of more than %d bitcoins.\r\n", tcount, mMinBalance );
				mBlockChain->printOldest(tcount,mMinBalance);
			}
			else if ( strcmp(argv[0],"zombie") == 0 )
			{
				uint32_t zdays = 365;
				if ( argc >=2 )
				{
					zdays = atoi(argv[1]);
					if ( zdays < 1 )
					{
						zdays = 1;
					}
				}
				printf("Generating Zombie Count for addresses older than %d days with a balance of more than %d bitcoins.\r\n", zdays, mMinBalance );
				mBlockChain->zombieReport(zdays,mMinBalance);
			}
			else if ( strcmp(argv[0],"by_day") == 0 )
			{
				mStatResolution = SR_DAY;
				printf("Gathering statistics every day.\r\n");
			}
			else if ( strcmp(argv[0],"by_month") == 0 )
			{
				mStatResolution = SR_MONTH;
				printf("Gathering statistics every month.\r\n");
			}
			else if ( strcmp(argv[0],"by_year") == 0 )
			{
				mStatResolution = SR_YEAR;
				printf("Gathering statistics every year.\r\n");
			}
			else if ( strcmp(argv[0],"counts") == 0 )
			{
				mBlockChain->reportCounts();
			}
			else if ( strcmp(argv[0],"block") == 0 )
			{
				if ( argc == 1 )
				{
					if ( mCurrentBlock )
					{
						mBlockChain->printBlock(mCurrentBlock);
						mCurrentBlock = mBlockChain->readBlock(mCurrentBlock->blockIndex+1);
					}
				}
				else
				{
					for (uint32_t i=1; i<argc; i++)
					{
						uint32_t index = (uint32_t)atoi(argv[i]);
						mCurrentBlock = getBlock(index);
						if ( mCurrentBlock )
						{
							mBlockChain->printBlock(mCurrentBlock);
						}
					}
				}
			}
		}
		switch ( mMode )
		{
			case CM_PROCESS:
				if ( mProcessBlock < mBlockChain->getBlockCount() )
				{
					mCurrentBlock = mBlockChain->readBlock(mProcessBlock);
					if ( mCurrentBlock && mProcessTransactions )
					{

						if ( mLastTime == 0 )
						{
							mLastTime = mCurrentBlock->timeStamp;
							mSatoshiTime = mCurrentBlock->timeStamp;
						}
						else
						{
							uint32_t currentTime = mCurrentBlock->timeStamp;
							time_t tnow(currentTime);
							struct tm beg;
							beg = *localtime(&tnow);
							time_t tbefore(mLastTime);
							struct tm before;
							before = *localtime(&tbefore);
							bool getStats = false;
							switch ( mStatResolution )
							{
								case SR_DAY:
									if ( beg.tm_yday != before.tm_yday )
									{
										getStats = true;
									}
									break;
								case SR_MONTH:
									if ( beg.tm_mon != before.tm_mon )
									{
										getStats = true;
									}
									break;
								case SR_YEAR:
									if ( beg.tm_year != before.tm_year )
									{
										getStats = true;
									}
									break;
							}
							if ( getStats )
							{
								const char *months[12] = { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };
								printf("Gathering statistics for %s %d, %d to %s %d, %d\r\n", 
									months[before.tm_mon], before.tm_mday, before.tm_year+1900,
									months[beg.tm_mon], beg.tm_mday, beg.tm_year+1900);
								mBlockChain->gatherStatistics(mLastTime,(uint32_t)zombieDate,mRecordAddresses);
								mLastTime = currentTime;
							}
						}
						mBlockChain->processTransactions(mCurrentBlock);  // process transactions into individual addresses
					}
					mProcessBlock++;
					if ( (mProcessBlock%10000) == 0 )
					{
						printf("Processed block #%d of %d total.\r\n", mProcessBlock, mBlockChain->getBlockCount() );
					}
				}
				else
				{
					printf("Finished processing all blocks in the blockchain.\r\n");
					mBlockChain->reportCounts();
					if ( mProcessTransactions )
					{
						printf("Gathering final statistics.\r\n");
						mBlockChain->gatherStatistics(mLastTime,(uint32_t)zombieDate,mRecordAddresses);
						printf("Saving statistics to file 'stats.csv\r\n");
						mBlockChain->saveStatistics(mRecordAddresses);
					}
					mMode = CM_NONE;
					mProcessBlock = 0;
				}
				break;
			case CM_SCAN:
				{
					bool ok = mBlockChain->readBlockHeaders(mMaxBlock,mLastBlockScan);
					if ( !ok )
					{
						mFinishedScanning = true;
						mMode = CM_NONE; // done scanning.
						mLastBlockScan = mBlockChain->buildBlockChain();
						printf("Finished scanning block headers. Built block-chain with %d blocks found..\r\n", mLastBlockScan);
						printf("To resolve transactions you must execute the 'process' command.\r\n");
						printf("To gather statistics so you can ouput balances of individual addresses, you must execute the 'statistics' command prior to running the process command.\r\n");
					}
				}
				break;
		}
		return mMode != CM_EXIT;
	}

	const BlockChain::Block *getBlock(uint32_t index)
	{
		mCurrentBlock = mBlockChain->readBlock(index);
		return mCurrentBlock;
	}

	CommandMode				mMode;
	bool					mRecordAddresses;
	bool					mFinishedScanning;
	bool					mProcessTransactions;
	StatResolution			mStatResolution;
	uint32_t				mProcessBlock;
	uint32_t				mMaxBlock;
	uint32_t				mLastBlockScan;
	uint32_t				mLastBlockPrint;
	const BlockChain::Block	*mCurrentBlock;
	BlockChain				*mBlockChain;
	uint32_t				mLastTime;
	uint32_t				mSatoshiTime;
	uint32_t				mMinBalance;
	BlockChainAddresses		*mAddresses;
};

int main(int argc,const char **argv)
{
	const char *dataPath = ".";
	if ( argc < 2 )
	{
		printf("Using local test FILE.\r\n");
	}
	else
	{
		dataPath = argv[1];
	}

	BlockChainCommand bc(dataPath);

	while ( bc.process() );


	return 0;
}
