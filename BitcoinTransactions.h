#ifndef BITCOIN_TRANSACTIONS_H

#define BITCOIN_TRANSACTIONS_H

#include "BlockChain.h"

// Work in progress; this is a code snippet which will take all input blocks in the bitcoin blockchain and convert them into a 
// sequence of ordered transactions for further processing.
//

class BitcoinTransactions
{
public:
	virtual void processBlock(const BlockChain::Block &block) = 0;
	virtual void release(void) = 0;
};


BitcoinTransactions *createTransactions(void);

#endif
