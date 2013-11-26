#include "BitcoinTransactions.h"
#include "BitcoinScript.h"
#include <stdio.h>

#ifdef _MSC_VER // Disable the stupid ass absurd warning messages from Visual Studio telling you that using stdlib and stdio is 'not valid ANSI C'
#pragma warning(disable:4996)
#endif



class BitcoinTransactionsImpl : public BitcoinTransactions
{
public:
	BitcoinTransactionsImpl(void)
	{
		mBitcoinScript = createBitcoinScript();
	}

	~BitcoinTransactionsImpl(void)
	{
		if ( mBitcoinScript )
		{
			mBitcoinScript->release();
		}
	}

	const char *hashString(const uint8_t *h)
	{
		static char temp[65];
		temp[64] = 0;
		for (uint32_t i=0; i<32; i++)
		{
			sprintf(&temp[i*2],"%02x", h[i] );
		}

		return temp;
	}

	virtual void processBlock(const BlockChain::Block &block)
	{
		uint64_t btc=0;
		uint64_t oneBTC = 100000000;

		printf("%d transactions.\r\n", block.transactionCount );
		for (uint32_t i=0; i<block.transactionCount; i++)
		{
			const BlockChain::BlockTransaction &t = block.transactions[i];
#if 0
			for (uint32_t i=0; i<t.inputCount; i++)
			{
				const BlockChain::BlockInput &input = t.inputs[i];
				mBitcoinScript->executeScript(input.responseScript,input.responseScriptLength);
			}
			for (uint32_t i=0; i<t.outputCount; i++)
			{
				const BlockChain::BlockOutput &ou tput = t.outputs[i];
				mBitcoinScript->resetStack();
				mBitcoinScript->executeScript(output.challengeScript,output.challengeScriptLength);
			}
#else
			printf("Transaction #%d has %d outputs and %d inputs.\r\n", i+1, t.outputCount, t.inputCount );
			for (uint32_t i=0; i<t.inputCount; i++)
			{
				const BlockChain::BlockInput &input = t.inputs[i];
				printf("Input %d : hash(%s) Index: %d\r\n", i+1, hashString(input.transactionHash), input.transactionIndex );
				mBitcoinScript->resetStack();
				mBitcoinScript->executeScript(input.responseScript,input.responseScriptLength);
			}
			for (uint32_t i=0; i<t.outputCount; i++)
			{
				const BlockChain::BlockOutput &output = t.outputs[i];
				printf("output %d is %d BTC.\r\n", i+1, (int)(output.value / oneBTC) );
				btc+=output.value;
				mBitcoinScript->resetStack();
				mBitcoinScript->executeScript(output.challengeScript,output.challengeScriptLength);
			}
#endif
		}
		printf("OutputBTC: %0.4f\r\n", (float)btc / (float)oneBTC );
	}

	virtual void release(void)
	{
		delete this;
	}

	BitcoinScript	*mBitcoinScript; // the bitcoin script parser
};


BitcoinTransactions *createTransactions(void)
{
	BitcoinTransactionsImpl *t = new BitcoinTransactionsImpl;
	return static_cast< BitcoinTransactions *>(t);
}
