#ifndef BLOCK_CHAIN_H

#define BLOCK_CHAIN_H


// This is a minimal C++ code snippet to read the bitcoin block chain one by one into memory.
//
// This code uses almost no dynamic memory allocation, no STL, no Boost, no containers (other than a very simple fixed size hash table for lookups),
// and it is only a few thousand lines of C++.
// It uses hard-coded arrays with fixed limits in their sizes.  This is because the actual data in the bitcoin blockchain
// does not ever exceed certain limits; so to avoid a great deal of unnecessary memory allocation this parser uses fixed buffers
// instead.
//
// Note, this code needs to be compiled on a 64bit machine as it now is able to load every single bitcoin transaction which has ever occurred
// into memory for analysis and reporting.  Obviously this uses quite a bit of memory.
//
// It's also pretty fast though, I suppose, it could be made faster, but not by a lot.  Some people might point out that it could be made faster
// if it cached the source block-chain file but, really, virtually all operating systems do that under the hood any way so 
// I doubt it would make much difference.  As it is, it reads one bitcoin blockchain 'block' into memory at a time, parses the transactions,
// inputs, and outputs, and returns them to the caller.  A person who would use this code snippet would, most likely, take this data
// and convert it into some other more reasonable format for further processing and analysis.  At least that's what I plan to do.
//
// On my machine the bitcoin blockchain data is comprised of 68 files each about 128mb apiece totaling 9.2gb.
//
// My machine parses this entire data set in roughly 95 seconds.
//
// It is important to note that this code assumes that you are running on a small-endian machine, like an X86.  It *does not* run on 
// big-endian machines (like a PowerPC for example).  If you have a big-endian machine, what can I say, get a real processor.
//
// http://en.wikipedia.org/wiki/Endianness
//
// This code snippet was written by John W. Ratcliff (jratcliffscarab@gmail.com) on June 30, 2013 on a lazy rainy Sunday afternoon 
//
// Note, this source was updated on January 1, 2014.  I worked on it over Christmas break.  It can now process every single transaction
// into memory, organize all public key addresses, and do some analysis on the blockchain.
//
// I wrote this code snippet for two reasons.  First, I just wanted to understand the bitcoin blockchain format myself and
// since I run the full bitcoin-qt client on my machine, I have all the data stored on my hard drive anyway.
//
// The second reason I wrote it is that I have an interest in doing some analysis on the block-chain data and, to do that, I first need to
// be able to parse the transactions.  When I looked at available resources on the internet they all use various scripting languages and 
// JSON-RPC.  That just screams stunningly, outrageously, absurdly, slow to me.
//
// The specific data I want to analyze it a study of the historical data value of outstanding and current bitcoins in circulation.
//
// There was an excellent paper published on this, but it was produced by processing an insane colliction of HTML files to get the output which
// makes their data almost instantly obsolete.
//
// Right now there is a big mystery about how many of the dormant bitcoins are irrevocably lost or are merely waiting to be cashed in at some
// point in the future.  There is really no way to know for sure, however, we can look at how many presumed dormant coins come out of hiding
// over time.  It's an interesting data mining exercise at any rate, and I wanted to be able to play around with exploring the dataset.
//
// Before I wrote this code snippet, like a good programmer, I first looked around the internet to see if I could just download something that would
// do the same thing.
//
// However, yet once again, I ran into the same nonsense that I always run into.  The most commonly referenced C++ code sample that shows how to read the blockchain
// has enormous dependencies and does not build out of the box.  I find this sort of thing terribly annoying.
//
// This code snippet is just a single header file and a single CPP.  In theory it should compile on any platform, all you have to do is revise a couple
// of typedefs at the top of the header to declare the basic int sizes on your platform.  It is well documented and, I believe, very easy to read and understand.
//
// That's it.  It doesn't use the STL, Boost, or another heavyweight dependences above and beyond standard library type stuff.
//
// I did find this excellent reference online; from which this code was written.  Think of this code snippet as essentially just a reference implementation
// of what is already covered on Jame's blog.
//
// If you find this code snippet useful; you can tip me at this bitcoin address:
//
// BITCOIN TIP JAR: "1BT66EoaGySkbY9J6MugvQRhMMXDwPxPya"
//
//http://james.lab6.com/2012/01/12/bitcoin-285-bytes-that-changed-the-world/
//
// https://en.bitcoin.it/wiki/Protocol_specification
//
// One problem with Jame's specifiction is it's not always super clear what the heirachy of the input data is; the classes in this header file
// should hopefully make that a bit more clear.
//
// An important note, a number of the inputs in the blockchain are marked as 'variable length integers' (presumablye to 'save space' even though they really don't)
// The variable length integer is capable of being as large as 64 bits but, in actuality, never is.
// That's why all of the integers in the following data structures are 32 bits in size.
//
// A couple of items; sometimes you can run out of blockchain data before you reach the end of the file.  Past a certain point the file just contains zeroes.
// This was not documented in Jame's page; but it is what I encounterd in the input data set.
//
// There are also many cases where the actual data in the block is a lot less than the reported block-length.  I'm going to assume that this too is normal
// and expected.

#include <stdint.h>	// Include stdint.h; available on most compilers but, if not, a copy is provided here for Microsoft Visual Studio

// This is the interface class for reading the BlockChain
class BlockChain
{
public:
	// Each transaction is comprised of a set of inputs.  This class defines that input data stream.
	class BlockInput
	{
	public:
		BlockInput(void)
		{
			responseScriptLength  = 0;
			responseScript = 0;
		}
		const uint8_t	*transactionHash;			// The hash of the input transaction; this a is a pointer to the 32 byte hash
		uint32_t		transactionIndex;			// The index of the transaction
		uint32_t		responseScriptLength;		// the length of the response script. (In theory this could be >32 bits; in practice it never will be.)
		const uint8_t	*responseScript;			// The response script.   This gets run on the bitcoin script virtual machine; see bitcoin docs
		uint32_t		sequenceNumber;				// The 'sequence' number
	};

	// Each transaction has a set of outputs; this class defines that output data stream.
	class BlockOutput
	{
	public:
		BlockOutput(void)
		{
			challengeScriptLength = 0;
			challengeScript = 0;
			isRipeMD160 = false;
		}
		uint64_t		value;					// value of the output (this is the actual value in BTC fixed decimal notation) @See bitcoin docs
		uint32_t		challengeScriptLength;	// The length of the challenge script  (In theory this could be >32 bits; in practice it never will be.)
		const uint8_t	*challengeScript;		// The contents of the challenge script.  This gets run on the bitcoin script virtual machine; see bitcoin docs
		bool			isRipeMD160;			// If this is true, then the public key is the 20 byte RIPEMD160 hash rather than the full 65 byte ECDSA hash
		const uint8_t	*publicKey;				// The public key output
	};

	// Each block contains a series of transactions; each transaction with it's own set of inputs and outputs.  
	// This class describes the transaction data.
	class BlockTransaction
	{
	public:
		BlockTransaction(void)
		{
			inputCount = 0;
			inputs = 0;
			outputCount = 0;
			outputs = 0;
			transactionIndex = 0;
		}
		uint32_t		transactionVersionNumber;	// The transaction version number
		uint32_t		inputCount;					// The number of inputs in the block; in theory this could be >32 bits; in practice it never will be.
		BlockInput		*inputs;					// A pointer to the array of inputs
		uint32_t		outputCount;				// The number of outputs in the block.
		BlockOutput		*outputs;					// The outputs in the block; 64bit unsigned int for each output; kind of a fixed decimal representation of bitcoin; see docs
		uint32_t		lockTime;					// The lock-time; currently always set to zero
		// This is data which is computed when the file is parsed; it is not contained in the block chain file itself.
		// This data can uniquely identify the specific transaction with information on how to go back to the seek location on disk and reread it
		uint8_t			transactionHash[32];		// This is the hash for this transaction
		uint32_t		transactionLength;			// The length of the data comprising this transaction.
		uint32_t		fileIndex;					// which blk?????.dat file this transaction is contained in.
		uint32_t		fileOffset;					// the seek file location of this transaction.
		uint32_t		transactionIndex;			// the sequential index number of this transaction
	};

	// This class defines a single block in the block chain.
	class Block
	{
	public:
		Block(void)
		{
			transactions = 0;
			transactionCount = 0;
			nextBlockHash = 0;
		}
		uint32_t		blockLength;				// the length of this block
		uint32_t		blockFormatVersion;			// The block format version
		const uint8_t	*previousBlockHash;			// A pointer to the previous block hash (32 bytes)
		const uint8_t	*merkleRoot;				// A pointer to the MerkleRoot hash
		uint32_t		timeStamp;					// The block timestamp in UNIX epoch time
		uint32_t		bits;						// This is the representation of the target; the value which the hash of the block header must not exceed in order to min the next block
		uint32_t		nonce;						// This is a random number generated during the mining process
		uint32_t		transactionCount;			// Number of transactions on this block
		BlockTransaction *transactions;				// The array of transactions in this block.
		// The following data items are not part of the block chain but are computed by convenience for the caller.
		uint8_t			computedBlockHash[32];		// The computed block hash
		uint32_t		blockIndex;					// Index of this block, the genesis block is considered zero
		uint32_t		totalInputCount;			// Total number of inputs in all transactions.
		uint32_t		totalOutputCount;			// Total number out outputs in all transaction.
		uint32_t		fileIndex;					// Which file index we are on.
		uint32_t		fileOffset;					// The file offset location where this block begins
		uint64_t		blockReward;				// Block redward in BTC
		const uint8_t	*nextBlockHash;				// The hash of the next block in the block chain; null if this is the last block
		bool			warning;					// there was a warning issued while processing this block.
	};

	virtual uint32_t getBlockCount(void) const = 0; // Return the number of blocks found
	virtual void printBlockHeaders(void) = 0;		// Print just the header information for all blocks

	virtual void printBlock(const Block *block) = 0; // prints the contents of the block to the console for debugging purposes

	// This will seek to a specific transaction in the blockchain and read it into memory.
	virtual const BlockTransaction *readSingleTransaction(const uint8_t *transactionHash) = 0;

	virtual const Block * readBlock(uint32_t blockIndex) = 0;	// use this method to read the next block in the block chain; if it returns null, the end of the block chain has been reached or there was a read error

	// This will consume a great deal of memory, do not call this routine unless you building for 64bit and have a lot of memory.
	virtual void processTransactions(const Block *b) = 0; // process the transactions in this block and assign them to individual wallets

	// Report the number of unique addresses used so far.
	virtual uint32_t gatherAddresses(void) = 0;

	// gather all relevant statistics for this time period.
	virtual void gatherStatistics(uint32_t stime,uint32_t zombieDate,bool record_addresses) = 0;

	virtual void printAddresses(void) = 0;
	virtual void printBlocks(void) = 0;

	virtual void saveStatistics(bool record_addresses) = 0;

	virtual void reportCounts(void) = 0;

	virtual void printTransactions(uint32_t blockIndex) = 0;

	virtual bool readBlockHeaders(uint32_t maxBlock,uint32_t &blockCount)= 0;
	virtual uint32_t buildBlockChain(void) = 0;

	virtual void printAddress(const char *address) = 0;
	virtual void printTopBalances(uint32_t tcount,uint32_t minBalance) = 0;
	virtual void printOldest(uint32_t tcount,uint32_t minBalance) = 0;
	virtual void zombieReport(uint32_t zdays,uint32_t minBalance) = 0;

	virtual void release(void) = 0;	// This method releases the block chain interface.
};


BlockChain *createBlockChain(const char *rootPath);	// Create the BlockChain interface using this root directory for the location of the first 'blk00000.dat' on your hard drive.

#endif
