#ifndef BLOCK_CHAIN_ADDRESSES_H

#define BLOCK_CHAIN_ADDRESSES_H

#include <stdint.h>



class BlockChainAddresses
{
public:
	class StatAddress
	{
	public:
		StatAddress(void)
		{
			mAddress = 0;
			mTotalReceived = 0;
			mTotalSent = 0;
			mLastTime = 0;
			mTransactionCount = 0;
			mInputCount = 0;
			mOutputCount = 0;
		}

		bool operator==(const StatAddress &a) const
		{
			return mAddress == a.mAddress &&
				mTotalSent == a.mTotalSent &&
				mTotalReceived == a.mTotalReceived &&
				mFirstTime == a.mFirstTime &&
				mLastTime == a.mLastTime &&
				mTransactionCount == a.mTransactionCount &&
				mInputCount == a.mInputCount &&
				mOutputCount == a.mOutputCount;
		}

		uint32_t getBalance(void) const
		{
			return mTotalReceived - mTotalSent;
		}

		uint32_t	mAddress;			// the address index.
		uint32_t	mTotalSent;			// total number of bitcoins sent
		uint32_t	mTotalReceived;
		uint32_t	mFirstTime;			// first time this address was used
		uint32_t	mLastTime;			// Last time a spend transaction occurred.
		uint8_t		mTransactionCount;	// total number of transactions
		uint8_t		mInputCount;		// total number of inputs.
		uint8_t		mOutputCount;		// total number of outputs
	};

	virtual void		printRow(void) = 0;				// Print out for debugging purposes the data in this current row
	virtual bool		seekRow(uint32_t index) = 0;	// Seek the file to this row location.
	virtual uint32_t	getRowCount(void) const = 0;	// return the number of rows found in the file.
	virtual void		release(void) = 0;				// release the interface
	virtual const StatAddress *getRow(uint32_t &scount) = 0;

protected:
	virtual ~BlockChainAddresses(void)
	{
	}
};


BlockChainAddresses *createBlockChainAddresses(const char *fname);

#endif
