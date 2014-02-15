#include "BlockChain.h"

//
// Written by John W. Ratcliff : mailto: jratcliffscarab@gmail.com
//
// Website:  http://codesuppository.blogspot.com/
//
// Source contained in this project includes portions of source code from other open source projects; though that source may have
// been modified to be included here.  Original notices are left in where appropriate.
//
// Some of the hash and bignumber implementations are based on source code find in the 'cbitcoin' project; though it has been modified here to remove all memory allocations.
//
// http://cbitcoin.com/
//
// If you find this code snippet useful; you can tip me at this bitcoin address:
//
// BITCOIN TIP JAR: "1BT66EoaGySkbY9J6MugvQRhMMXDwPxPya"
//

#ifdef _MSC_VER // Disable the stupid ass absurd warning messages from Visual Studio telling you that using stdlib and stdio is 'not valid ANSI C'
#pragma warning(disable:4996)
#pragma warning(disable:4718)
#endif

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <time.h>

// Note, to minimize dynamic memory allocation this parser pre-allocates memory for the maximum ever expected number
// of bitcoin addresses, transactions, inputs, outputs, and blocks.
// The numbers here are large enough to read the entire blockchain as of January 1, 2014 with a fair amoutn of room to grow.
// However, they will need to be increased over time as the blockchain grows.
// Dynamic memory allocation isn't free, every time you dynamically allocate memory there is a significant overhead; so by pre-allocating
// all of the memory needed into one continguous block you actually save an enormous amount of memory overall and also make the code run
// orders of magnitude faster.

#define SMALL_MEMORY_PROFILE 0 // a debug option so I can run/test the code on a small memory configuration machine  If this is

#if SMALL_MEMORY_PROFILE

#define MAX_BITCOIN_ADDRESSES 4000000 // 4 million unique addresses.
#define MAX_TOTAL_TRANSACTIONS 500000 // 500,000
#define MAX_TOTAL_INPUTS  1000000 //  one million
#define MAX_TOTAL_OUTPUTS 1000000 // one million
#define MAX_TOTAL_BLOCKS 150000		// 150,000 blocks

#else

#define MAX_BITCOIN_ADDRESSES 40000000 // 40 million unique addresses.
#define MAX_TOTAL_TRANSACTIONS 40000000 // 40 million transactions.
#define MAX_TOTAL_INPUTS 100000000 // 150 million inputs.
#define MAX_TOTAL_OUTPUTS 100000000 // 150 million outputs
#define MAX_TOTAL_BLOCKS 500000		// 1/2 million blocks.

#endif

// Some globals for error reporting.
static uint32_t gBlockIndex=0;
static uint32_t gTransactionIndex=0;
static uint32_t gOutputIndex=0;
static bool		gIsWarning=false;

class Hash256
{
public:
	Hash256(void)
	{
		mWord0 = 0;
		mWord1 = 0;
		mWord2 = 0;
		mWord3 = 0;
	}

	Hash256(const Hash256 &h)
	{
		mWord0 = h.mWord0;
		mWord1 = h.mWord1;
		mWord2 = h.mWord2;
		mWord3 = h.mWord3;
	}

	inline Hash256(const uint8_t *src)
	{
		mWord0 = *(const uint64_t *)(src);
		mWord1 = *(const uint64_t *)(src+8);
		mWord2 = *(const uint64_t *)(src+16);
		mWord3 = *(const uint64_t *)(src+24);
	}

	inline uint32_t getHash(void) const
	{
		const uint32_t *h = (const uint32_t *)&mWord0;
		return h[0] ^ h[1] ^ h[2] ^ h[3] ^ h[4] ^ h[5] ^ h[6] ^ h[7];
	}

	inline bool operator==(const Hash256 &h) const
	{
		return mWord0 == h.mWord0 && mWord1 == h.mWord1 && mWord2 == h.mWord2 && mWord3 == h.mWord3;
	}


	uint64_t	mWord0;
	uint64_t	mWord1;
	uint64_t	mWord2;
	uint64_t	mWord3;
};



class BlockHeader : public Hash256
{
public:
	BlockHeader(void)
	{
		mFileIndex = 0;
		mFileOffset = 0;
		mBlockLength = 0;
	}
	BlockHeader(const Hash256 &h) : Hash256(h)
	{
		mFileIndex = 0;
		mFileOffset = 0;
		mBlockLength = 0;
	}
	uint32_t	mFileIndex;
	uint32_t	mFileOffset;
	uint32_t	mBlockLength;
	uint8_t		mPreviousBlockHash[32];
};

struct BlockPrefix
{
	uint32_t	mVersion;					// The block version number.
	uint8_t		mPreviousBlock[32];			// The 32 byte (256 bit) hash of the previous block in the blockchain
	uint8_t		mMerkleRoot[32];			// The 32 bye merkle root hash
	uint32_t	mTimeStamp;					// The block time stamp
	uint32_t	mBits;						// The block bits field.
	uint32_t	mNonce;						// The block random number 'nonce' field.
};



static const char *getTimeString(uint32_t timeStamp)
{
	if ( timeStamp == 0 )
	{
		return "NEVER";
	}
	static char scratch[1024];
	time_t t(timeStamp);
	struct tm *gtm = gmtime(&t);
	strftime(scratch, 1024, "%m/%d/%Y %H:%M:%S", gtm);
	return scratch;
}


#define MAXNUMERIC 32  // JWR  support up to 16 32 character long numeric formated strings
#define MAXFNUM    16

static	char  gFormat[MAXNUMERIC*MAXFNUM];
static int32_t    gIndex=0;

static const char * formatNumber(int32_t number) // JWR  format this integer into a fancy comma delimited string
{
	char * dest = &gFormat[gIndex*MAXNUMERIC];
	gIndex++;
	if ( gIndex == MAXFNUM ) gIndex = 0;

	char scratch[512];

#ifdef _MSC_VER
	itoa(number,scratch,10);
#else
	snprintf(scratch, 10, "%d", number);
#endif

	char *source = scratch;
	char *str = dest;
	uint32_t len = (uint32_t)strlen(scratch);
	if ( scratch[0] == '-' )
	{
		*str++ = '-';
		source++;
		len--;
	}
	for (uint32_t i=0; i<len; i++)
	{
		int32_t place = (len-1)-i;
		*str++ = source[i];
		if ( place && (place%3) == 0 ) *str++ = ',';
	}
	*str = 0;

	return dest;
}



template < class Key,
	uint32_t hashTableSize = 512,		// *MUST* be a power of 2!
	uint32_t hashTableEntries = 2048 >

class SimpleHash
{
public:
	class HashEntry
	{
	public:
		HashEntry(void)
		{
			mNext = NULL;
		}
		Key			mKey;
		HashEntry*	mNext;
	};

	class Iterator
	{
		friend class SimpleHash;
	public:
		Iterator(void)
		{
			mHashIndex = 0;
			mEntry = NULL;
		}

		Key * first(void) const
		{
			return mEntry ? &mEntry->mKey : NULL;
		}

		bool empty(void) const
		{
			return mEntry == NULL ? true : false;
		}

	private:
		uint32_t	mHashIndex;
		HashEntry	*mEntry;
	};

	inline Iterator begin(void) const
	{
		Iterator ret;
		for (uint32_t i=0; i<hashTableSize; i++)
		{
			if ( mHashTable[i] )
			{
				ret.mHashIndex = i;
				ret.mEntry = mHashTable[i];
				break;
			}
		}
		return ret;
	}

	inline bool next(Iterator &iter) const
	{
		bool ret = false;

		if ( iter.mEntry && iter.mEntry->mNext )
		{
			iter.mEntry = iter.mEntry->mNext;
			ret = true;
		}
		else
		{
			iter.mEntry = NULL;
			for (uint32_t i=iter.mHashIndex+1; i<hashTableSize; i++)
			{
				if ( mHashTable[i] )
				{
					iter.mHashIndex = i;
					iter.mEntry = mHashTable[i];
					ret = true;
					break;
				}
			}
		}
		return ret;
	}

	inline bool empty(void) const
	{
		return mHashTableCount ? true : false;
	}

	SimpleHash(void)
	{
		mEntries = NULL;
		mHashTableCount = 0;
		for (uint32_t i = 0; i < hashTableSize; i++)
		{
			mHashTable[i] = NULL;
		}
	}

	inline void init(void)
	{
		if ( mEntries == NULL )
		{
			mEntries = new HashEntry[hashTableEntries];
		}
	}

	~SimpleHash(void)
	{
		delete []mEntries;
	}

	inline uint32_t getIndex(const Key *k) const
	{
		assert(k);
		HashEntry *h = (HashEntry *)k;
		return (uint32_t)(h-mEntries);
	}

	inline Key * getKey(uint32_t i) const
	{
		Key *ret = NULL;
		assert( i < mHashTableCount );
		if ( i < mHashTableCount )
		{
			ret = &mEntries[i].mKey;
		}
		return ret;
	}

	inline Key* find(const Key& key)  const
	{
		Key* ret = NULL;
		uint32_t hash = getHash(key);
		HashEntry* h = mHashTable[hash];
		while (h)
		{
			if (h->mKey == key)
			{
				ret = &h->mKey;
				break;
			}
			h = h->mNext;
		}
		return ret;
	}

	// Inserts are not thread safe; use a mutex
	inline Key * insert(const Key& key)
	{
		Key *ret = NULL;
		init(); // allocate the entries table
		if (mHashTableCount < hashTableEntries)
		{
			HashEntry* h = &mEntries[mHashTableCount];
			h->mKey = key;
			ret = &h->mKey;
			mHashTableCount++;
			uint32_t hash = getHash(key);
			if (mHashTable[hash])
			{
				HashEntry* next = mHashTable[hash];
				mHashTable[hash] = h;
				h->mNext = next;
			}
			else
			{
				mHashTable[hash] = h;
			}
		}
		else
		{
			assert(0); // we should never run out of hash entries
		}
		return ret;
	}

	inline uint32_t size(void) const
	{
		return mHashTableCount;
	}
private:

	inline uint32_t getHash(const Key& key) const
	{
		return key.getHash() & (hashTableSize - 1);
	}


	HashEntry*		mHashTable[hashTableSize];
	unsigned int	mHashTableCount;
	HashEntry		*mEntries;

};


class FileLocation : public Hash256
{
public:
	FileLocation(void)
	{

	}
	FileLocation(const Hash256 &h,uint32_t fileIndex,uint32_t fileOffset,uint32_t fileLength,uint32_t transactionIndex) : Hash256(h)
	{
		mFileIndex = fileIndex;
		mFileOffset = fileOffset;
		mFileLength = fileLength;
		mTransactionIndex = transactionIndex;
	}
	uint32_t	mFileIndex;
	uint32_t	mFileOffset;
	uint32_t	mFileLength;
	uint32_t	mTransactionIndex;
};

typedef SimpleHash< FileLocation, 4194304, MAX_TOTAL_TRANSACTIONS > TransactionHashMap;
typedef SimpleHash< BlockHeader, 65536, MAX_TOTAL_BLOCKS > BlockHeaderMap;

//*********** Begin of Source Code for RIPEMD160 hash *********************************
namespace BLOCKCHAIN_RIPEMD160
{
/********************************************************************/
/* macro definitions */

/* collect four bytes into one word: */
#define BYTES_TO_DWORD(strptr)                    \
            (((uint32_t) *((strptr)+3) << 24) | \
             ((uint32_t) *((strptr)+2) << 16) | \
             ((uint32_t) *((strptr)+1) <<  8) | \
             ((uint32_t) *(strptr)))

/* ROL(x, n) cyclically rotates x over n bits to the left */
/* x must be of an unsigned 32 bits type and 0 <= n < 32. */
#define ROL(x, n)        (((x) << (n)) | ((x) >> (32-(n))))

/* the five basic functions F(), G() and H() */
#define F(x, y, z)        ((x) ^ (y) ^ (z)) 
#define G(x, y, z)        (((x) & (y)) | (~(x) & (z))) 
#define H(x, y, z)        (((x) | ~(y)) ^ (z))
#define I(x, y, z)        (((x) & (z)) | ((y) & ~(z))) 
#define J(x, y, z)        ((x) ^ ((y) | ~(z)))
  
/* the ten basic operations FF() through III() */
#define FF(a, b, c, d, e, x, s)        {\
      (a) += F((b), (c), (d)) + (x);\
      (a) = ROL((a), (s)) + (e);\
      (c) = ROL((c), 10);\
   }
#define GG(a, b, c, d, e, x, s)        {\
      (a) += G((b), (c), (d)) + (x) + 0x5a827999UL;\
      (a) = ROL((a), (s)) + (e);\
      (c) = ROL((c), 10);\
   }
#define HH(a, b, c, d, e, x, s)        {\
      (a) += H((b), (c), (d)) + (x) + 0x6ed9eba1UL;\
      (a) = ROL((a), (s)) + (e);\
      (c) = ROL((c), 10);\
   }
#define II(a, b, c, d, e, x, s)        {\
      (a) += I((b), (c), (d)) + (x) + 0x8f1bbcdcUL;\
      (a) = ROL((a), (s)) + (e);\
      (c) = ROL((c), 10);\
   }
#define JJ(a, b, c, d, e, x, s)        {\
      (a) += J((b), (c), (d)) + (x) + 0xa953fd4eUL;\
      (a) = ROL((a), (s)) + (e);\
      (c) = ROL((c), 10);\
   }
#define FFF(a, b, c, d, e, x, s)        {\
      (a) += F((b), (c), (d)) + (x);\
      (a) = ROL((a), (s)) + (e);\
      (c) = ROL((c), 10);\
   }
#define GGG(a, b, c, d, e, x, s)        {\
      (a) += G((b), (c), (d)) + (x) + 0x7a6d76e9UL;\
      (a) = ROL((a), (s)) + (e);\
      (c) = ROL((c), 10);\
   }
#define HHH(a, b, c, d, e, x, s)        {\
      (a) += H((b), (c), (d)) + (x) + 0x6d703ef3UL;\
      (a) = ROL((a), (s)) + (e);\
      (c) = ROL((c), 10);\
   }
#define III(a, b, c, d, e, x, s)        {\
      (a) += I((b), (c), (d)) + (x) + 0x5c4dd124UL;\
      (a) = ROL((a), (s)) + (e);\
      (c) = ROL((c), 10);\
   }
#define JJJ(a, b, c, d, e, x, s)        {\
      (a) += J((b), (c), (d)) + (x) + 0x50a28be6UL;\
      (a) = ROL((a), (s)) + (e);\
      (c) = ROL((c), 10);\
   }

/********************************************************************/

/* function prototypes */

void MDinit(uint32_t *MDbuf);
/*
 *  initializes MDbuffer to "magic constants"
 */

void compress(uint32_t *MDbuf, uint32_t *X);
/*
 *  the compression function.
 *  transforms MDbuf using message bytes X[0] through X[15]
 */

void MDfinish(uint32_t *MDbuf,const uint8_t *strptr, uint32_t lswlen, uint32_t mswlen);
/*
 *  puts bytes from strptr into X and pad out; appends length 
 *  and finally, compresses the last block(s)
 *  note: length in bits == 8 * (lswlen + 2^32 mswlen).
 *  note: there are (lswlen mod 64) bytes left in strptr.
 */

/********************************************************************/

void MDinit(uint32_t *MDbuf)
{
   MDbuf[0] = 0x67452301UL;
   MDbuf[1] = 0xefcdab89UL;
   MDbuf[2] = 0x98badcfeUL;
   MDbuf[3] = 0x10325476UL;
   MDbuf[4] = 0xc3d2e1f0UL;

   return;
}

/********************************************************************/

void compress(uint32_t *MDbuf, uint32_t *X)
{
   uint32_t aa = MDbuf[0],  bb = MDbuf[1],  cc = MDbuf[2],
         dd = MDbuf[3],  ee = MDbuf[4];
   uint32_t aaa = MDbuf[0], bbb = MDbuf[1], ccc = MDbuf[2],
         ddd = MDbuf[3], eee = MDbuf[4];

   /* round 1 */
   FF(aa, bb, cc, dd, ee, X[ 0], 11);
   FF(ee, aa, bb, cc, dd, X[ 1], 14);
   FF(dd, ee, aa, bb, cc, X[ 2], 15);
   FF(cc, dd, ee, aa, bb, X[ 3], 12);
   FF(bb, cc, dd, ee, aa, X[ 4],  5);
   FF(aa, bb, cc, dd, ee, X[ 5],  8);
   FF(ee, aa, bb, cc, dd, X[ 6],  7);
   FF(dd, ee, aa, bb, cc, X[ 7],  9);
   FF(cc, dd, ee, aa, bb, X[ 8], 11);
   FF(bb, cc, dd, ee, aa, X[ 9], 13);
   FF(aa, bb, cc, dd, ee, X[10], 14);
   FF(ee, aa, bb, cc, dd, X[11], 15);
   FF(dd, ee, aa, bb, cc, X[12],  6);
   FF(cc, dd, ee, aa, bb, X[13],  7);
   FF(bb, cc, dd, ee, aa, X[14],  9);
   FF(aa, bb, cc, dd, ee, X[15],  8);
                             
   /* round 2 */
   GG(ee, aa, bb, cc, dd, X[ 7],  7);
   GG(dd, ee, aa, bb, cc, X[ 4],  6);
   GG(cc, dd, ee, aa, bb, X[13],  8);
   GG(bb, cc, dd, ee, aa, X[ 1], 13);
   GG(aa, bb, cc, dd, ee, X[10], 11);
   GG(ee, aa, bb, cc, dd, X[ 6],  9);
   GG(dd, ee, aa, bb, cc, X[15],  7);
   GG(cc, dd, ee, aa, bb, X[ 3], 15);
   GG(bb, cc, dd, ee, aa, X[12],  7);
   GG(aa, bb, cc, dd, ee, X[ 0], 12);
   GG(ee, aa, bb, cc, dd, X[ 9], 15);
   GG(dd, ee, aa, bb, cc, X[ 5],  9);
   GG(cc, dd, ee, aa, bb, X[ 2], 11);
   GG(bb, cc, dd, ee, aa, X[14],  7);
   GG(aa, bb, cc, dd, ee, X[11], 13);
   GG(ee, aa, bb, cc, dd, X[ 8], 12);

   /* round 3 */
   HH(dd, ee, aa, bb, cc, X[ 3], 11);
   HH(cc, dd, ee, aa, bb, X[10], 13);
   HH(bb, cc, dd, ee, aa, X[14],  6);
   HH(aa, bb, cc, dd, ee, X[ 4],  7);
   HH(ee, aa, bb, cc, dd, X[ 9], 14);
   HH(dd, ee, aa, bb, cc, X[15],  9);
   HH(cc, dd, ee, aa, bb, X[ 8], 13);
   HH(bb, cc, dd, ee, aa, X[ 1], 15);
   HH(aa, bb, cc, dd, ee, X[ 2], 14);
   HH(ee, aa, bb, cc, dd, X[ 7],  8);
   HH(dd, ee, aa, bb, cc, X[ 0], 13);
   HH(cc, dd, ee, aa, bb, X[ 6],  6);
   HH(bb, cc, dd, ee, aa, X[13],  5);
   HH(aa, bb, cc, dd, ee, X[11], 12);
   HH(ee, aa, bb, cc, dd, X[ 5],  7);
   HH(dd, ee, aa, bb, cc, X[12],  5);

   /* round 4 */
   II(cc, dd, ee, aa, bb, X[ 1], 11);
   II(bb, cc, dd, ee, aa, X[ 9], 12);
   II(aa, bb, cc, dd, ee, X[11], 14);
   II(ee, aa, bb, cc, dd, X[10], 15);
   II(dd, ee, aa, bb, cc, X[ 0], 14);
   II(cc, dd, ee, aa, bb, X[ 8], 15);
   II(bb, cc, dd, ee, aa, X[12],  9);
   II(aa, bb, cc, dd, ee, X[ 4],  8);
   II(ee, aa, bb, cc, dd, X[13],  9);
   II(dd, ee, aa, bb, cc, X[ 3], 14);
   II(cc, dd, ee, aa, bb, X[ 7],  5);
   II(bb, cc, dd, ee, aa, X[15],  6);
   II(aa, bb, cc, dd, ee, X[14],  8);
   II(ee, aa, bb, cc, dd, X[ 5],  6);
   II(dd, ee, aa, bb, cc, X[ 6],  5);
   II(cc, dd, ee, aa, bb, X[ 2], 12);

   /* round 5 */
   JJ(bb, cc, dd, ee, aa, X[ 4],  9);
   JJ(aa, bb, cc, dd, ee, X[ 0], 15);
   JJ(ee, aa, bb, cc, dd, X[ 5],  5);
   JJ(dd, ee, aa, bb, cc, X[ 9], 11);
   JJ(cc, dd, ee, aa, bb, X[ 7],  6);
   JJ(bb, cc, dd, ee, aa, X[12],  8);
   JJ(aa, bb, cc, dd, ee, X[ 2], 13);
   JJ(ee, aa, bb, cc, dd, X[10], 12);
   JJ(dd, ee, aa, bb, cc, X[14],  5);
   JJ(cc, dd, ee, aa, bb, X[ 1], 12);
   JJ(bb, cc, dd, ee, aa, X[ 3], 13);
   JJ(aa, bb, cc, dd, ee, X[ 8], 14);
   JJ(ee, aa, bb, cc, dd, X[11], 11);
   JJ(dd, ee, aa, bb, cc, X[ 6],  8);
   JJ(cc, dd, ee, aa, bb, X[15],  5);
   JJ(bb, cc, dd, ee, aa, X[13],  6);

   /* parallel round 1 */
   JJJ(aaa, bbb, ccc, ddd, eee, X[ 5],  8);
   JJJ(eee, aaa, bbb, ccc, ddd, X[14],  9);
   JJJ(ddd, eee, aaa, bbb, ccc, X[ 7],  9);
   JJJ(ccc, ddd, eee, aaa, bbb, X[ 0], 11);
   JJJ(bbb, ccc, ddd, eee, aaa, X[ 9], 13);
   JJJ(aaa, bbb, ccc, ddd, eee, X[ 2], 15);
   JJJ(eee, aaa, bbb, ccc, ddd, X[11], 15);
   JJJ(ddd, eee, aaa, bbb, ccc, X[ 4],  5);
   JJJ(ccc, ddd, eee, aaa, bbb, X[13],  7);
   JJJ(bbb, ccc, ddd, eee, aaa, X[ 6],  7);
   JJJ(aaa, bbb, ccc, ddd, eee, X[15],  8);
   JJJ(eee, aaa, bbb, ccc, ddd, X[ 8], 11);
   JJJ(ddd, eee, aaa, bbb, ccc, X[ 1], 14);
   JJJ(ccc, ddd, eee, aaa, bbb, X[10], 14);
   JJJ(bbb, ccc, ddd, eee, aaa, X[ 3], 12);
   JJJ(aaa, bbb, ccc, ddd, eee, X[12],  6);

   /* parallel round 2 */
   III(eee, aaa, bbb, ccc, ddd, X[ 6],  9); 
   III(ddd, eee, aaa, bbb, ccc, X[11], 13);
   III(ccc, ddd, eee, aaa, bbb, X[ 3], 15);
   III(bbb, ccc, ddd, eee, aaa, X[ 7],  7);
   III(aaa, bbb, ccc, ddd, eee, X[ 0], 12);
   III(eee, aaa, bbb, ccc, ddd, X[13],  8);
   III(ddd, eee, aaa, bbb, ccc, X[ 5],  9);
   III(ccc, ddd, eee, aaa, bbb, X[10], 11);
   III(bbb, ccc, ddd, eee, aaa, X[14],  7);
   III(aaa, bbb, ccc, ddd, eee, X[15],  7);
   III(eee, aaa, bbb, ccc, ddd, X[ 8], 12);
   III(ddd, eee, aaa, bbb, ccc, X[12],  7);
   III(ccc, ddd, eee, aaa, bbb, X[ 4],  6);
   III(bbb, ccc, ddd, eee, aaa, X[ 9], 15);
   III(aaa, bbb, ccc, ddd, eee, X[ 1], 13);
   III(eee, aaa, bbb, ccc, ddd, X[ 2], 11);

   /* parallel round 3 */
   HHH(ddd, eee, aaa, bbb, ccc, X[15],  9);
   HHH(ccc, ddd, eee, aaa, bbb, X[ 5],  7);
   HHH(bbb, ccc, ddd, eee, aaa, X[ 1], 15);
   HHH(aaa, bbb, ccc, ddd, eee, X[ 3], 11);
   HHH(eee, aaa, bbb, ccc, ddd, X[ 7],  8);
   HHH(ddd, eee, aaa, bbb, ccc, X[14],  6);
   HHH(ccc, ddd, eee, aaa, bbb, X[ 6],  6);
   HHH(bbb, ccc, ddd, eee, aaa, X[ 9], 14);
   HHH(aaa, bbb, ccc, ddd, eee, X[11], 12);
   HHH(eee, aaa, bbb, ccc, ddd, X[ 8], 13);
   HHH(ddd, eee, aaa, bbb, ccc, X[12],  5);
   HHH(ccc, ddd, eee, aaa, bbb, X[ 2], 14);
   HHH(bbb, ccc, ddd, eee, aaa, X[10], 13);
   HHH(aaa, bbb, ccc, ddd, eee, X[ 0], 13);
   HHH(eee, aaa, bbb, ccc, ddd, X[ 4],  7);
   HHH(ddd, eee, aaa, bbb, ccc, X[13],  5);

   /* parallel round 4 */   
   GGG(ccc, ddd, eee, aaa, bbb, X[ 8], 15);
   GGG(bbb, ccc, ddd, eee, aaa, X[ 6],  5);
   GGG(aaa, bbb, ccc, ddd, eee, X[ 4],  8);
   GGG(eee, aaa, bbb, ccc, ddd, X[ 1], 11);
   GGG(ddd, eee, aaa, bbb, ccc, X[ 3], 14);
   GGG(ccc, ddd, eee, aaa, bbb, X[11], 14);
   GGG(bbb, ccc, ddd, eee, aaa, X[15],  6);
   GGG(aaa, bbb, ccc, ddd, eee, X[ 0], 14);
   GGG(eee, aaa, bbb, ccc, ddd, X[ 5],  6);
   GGG(ddd, eee, aaa, bbb, ccc, X[12],  9);
   GGG(ccc, ddd, eee, aaa, bbb, X[ 2], 12);
   GGG(bbb, ccc, ddd, eee, aaa, X[13],  9);
   GGG(aaa, bbb, ccc, ddd, eee, X[ 9], 12);
   GGG(eee, aaa, bbb, ccc, ddd, X[ 7],  5);
   GGG(ddd, eee, aaa, bbb, ccc, X[10], 15);
   GGG(ccc, ddd, eee, aaa, bbb, X[14],  8);

   /* parallel round 5 */
   FFF(bbb, ccc, ddd, eee, aaa, X[12] ,  8);
   FFF(aaa, bbb, ccc, ddd, eee, X[15] ,  5);
   FFF(eee, aaa, bbb, ccc, ddd, X[10] , 12);
   FFF(ddd, eee, aaa, bbb, ccc, X[ 4] ,  9);
   FFF(ccc, ddd, eee, aaa, bbb, X[ 1] , 12);
   FFF(bbb, ccc, ddd, eee, aaa, X[ 5] ,  5);
   FFF(aaa, bbb, ccc, ddd, eee, X[ 8] , 14);
   FFF(eee, aaa, bbb, ccc, ddd, X[ 7] ,  6);
   FFF(ddd, eee, aaa, bbb, ccc, X[ 6] ,  8);
   FFF(ccc, ddd, eee, aaa, bbb, X[ 2] , 13);
   FFF(bbb, ccc, ddd, eee, aaa, X[13] ,  6);
   FFF(aaa, bbb, ccc, ddd, eee, X[14] ,  5);
   FFF(eee, aaa, bbb, ccc, ddd, X[ 0] , 15);
   FFF(ddd, eee, aaa, bbb, ccc, X[ 3] , 13);
   FFF(ccc, ddd, eee, aaa, bbb, X[ 9] , 11);
   FFF(bbb, ccc, ddd, eee, aaa, X[11] , 11);

   /* combine results */
   ddd += cc + MDbuf[1];               /* final result for MDbuf[0] */
   MDbuf[1] = MDbuf[2] + dd + eee;
   MDbuf[2] = MDbuf[3] + ee + aaa;
   MDbuf[3] = MDbuf[4] + aa + bbb;
   MDbuf[4] = MDbuf[0] + bb + ccc;
   MDbuf[0] = ddd;

   return;
}

/********************************************************************/

void MDfinish(uint32_t *MDbuf,const uint8_t *strptr, uint32_t lswlen, uint32_t mswlen)
{
   unsigned int i;                                 /* counter       */
   uint32_t        X[16];                             /* message words */

   memset(X, 0, 16*sizeof(uint32_t));

   /* put bytes from strptr into X */
   for (i=0; i<(lswlen&63); i++) {
      /* uint8_t i goes into word X[i div 4] at pos.  8*(i mod 4)  */
      X[i>>2] ^= (uint32_t) *strptr++ << (8 * (i&3));
   }

   /* append the bit m_n == 1 */
   X[(lswlen>>2)&15] ^= (uint32_t)1 << (8*(lswlen&3) + 7);

   if ((lswlen & 63) > 55) {
      /* length goes to next block */
      compress(MDbuf, X);
      memset(X, 0, 16*sizeof(uint32_t));
   }

   /* append length in bits*/
   X[14] = lswlen << 3;
   X[15] = (lswlen >> 29) | (mswlen << 3);
   compress(MDbuf, X);

   return;
}

#define RMDsize 160

void computeRIPEMD160(const void *_message,uint32_t length,uint8_t hashcode[20])
/*
 * returns RMD(message)
 * message should be a string terminated by '\0'
 */
{
	const uint8_t *message = (const uint8_t *)_message;
	uint32_t         MDbuf[RMDsize/32];   /* contains (A, B, C, D(, E))   */
	uint32_t         X[16];               /* current 16-word chunk        */

	/* initialize */
	MDinit(MDbuf);

	/* process message in 16-word chunks */
	for (uint32_t nbytes=length; nbytes > 63; nbytes-=64) 
	{
		for (uint32_t i=0; i<16; i++) 
		{
			X[i] = BYTES_TO_DWORD(message);
			message += 4;
		}
		compress(MDbuf, X);
	}	/* length mod 64 bytes left */

	/* finish: */
	MDfinish(MDbuf, message, length, 0);

	for (uint32_t i=0; i<RMDsize/8; i+=4) 
	{
		hashcode[i]   = (uint8_t)(MDbuf[i>>2]);         /* implicit cast to uint8_t  */
		hashcode[i+1] = (uint8_t)(MDbuf[i>>2] >>  8);  /*  extracts the 8 least  */
		hashcode[i+2] = (uint8_t)(MDbuf[i>>2] >> 16);  /*  significant bits.     */
		hashcode[i+3] = (uint8_t)(MDbuf[i>>2] >> 24);
	}

}


}; // end of RIPEMD160 hash

//********** Beginning of source code for SHA256 hash

namespace BLOCKCHAIN_SHA256
{
#define SHA256_HASH_SIZE  32	/* 256 bit */
#define SHA256_HASH_WORDS 8
#define SHA256_UNROLL 64	// This define determines how much loop unrolling is done when computing the hash; 

	// Uncomment this line of code if you want this snippet to compute the endian mode of your processor at run time; rather than at compile time.
	//#define RUNTIME_ENDIAN

	// Uncomment this line of code if you want this routine to compile for a big-endian processor
	//#define WORDS_BIGENDIAN

	typedef struct 
	{
		uint64_t totalLength;
		uint32_t hash[SHA256_HASH_WORDS];
		uint32_t bufferLength;
		union 
		{
			uint32_t words[16];
			uint8_t bytes[64];
		} buffer;
	} sha256_ctx_t;

	void sha256_init(sha256_ctx_t * sc);
	void sha256_update(sha256_ctx_t * sc, const void *data, uint32_t len);
	void sha256_finalize(sha256_ctx_t * sc, uint8_t hash[SHA256_HASH_SIZE]);


#define ROTL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))
#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

#define Ch(x, y, z) ((z) ^ ((x) & ((y) ^ (z))))
#define Maj(x, y, z) (((x) & ((y) | (z))) | ((y) & (z)))
#define SIGMA0(x) (ROTR((x), 2) ^ ROTR((x), 13) ^ ROTR((x), 22))
#define SIGMA1(x) (ROTR((x), 6) ^ ROTR((x), 11) ^ ROTR((x), 25))
#define sigma0(x) (ROTR((x), 7) ^ ROTR((x), 18) ^ ((x) >> 3))
#define sigma1(x) (ROTR((x), 17) ^ ROTR((x), 19) ^ ((x) >> 10))

#define DO_ROUND() {							\
	t1 = h + SIGMA1(e) + Ch(e, f, g) + *(Kp++) + *(W++);	\
	t2 = SIGMA0(a) + Maj(a, b, c);				\
	h = g;							\
	g = f;							\
	f = e;							\
	e = d + t1;						\
	d = c;							\
	c = b;							\
	b = a;							\
	a = t1 + t2;						\
	}

	static const uint32_t K[64] = {
		0x428a2f98L, 0x71374491L, 0xb5c0fbcfL, 0xe9b5dba5L,
		0x3956c25bL, 0x59f111f1L, 0x923f82a4L, 0xab1c5ed5L,
		0xd807aa98L, 0x12835b01L, 0x243185beL, 0x550c7dc3L,
		0x72be5d74L, 0x80deb1feL, 0x9bdc06a7L, 0xc19bf174L,
		0xe49b69c1L, 0xefbe4786L, 0x0fc19dc6L, 0x240ca1ccL,
		0x2de92c6fL, 0x4a7484aaL, 0x5cb0a9dcL, 0x76f988daL,
		0x983e5152L, 0xa831c66dL, 0xb00327c8L, 0xbf597fc7L,
		0xc6e00bf3L, 0xd5a79147L, 0x06ca6351L, 0x14292967L,
		0x27b70a85L, 0x2e1b2138L, 0x4d2c6dfcL, 0x53380d13L,
		0x650a7354L, 0x766a0abbL, 0x81c2c92eL, 0x92722c85L,
		0xa2bfe8a1L, 0xa81a664bL, 0xc24b8b70L, 0xc76c51a3L,
		0xd192e819L, 0xd6990624L, 0xf40e3585L, 0x106aa070L,
		0x19a4c116L, 0x1e376c08L, 0x2748774cL, 0x34b0bcb5L,
		0x391c0cb3L, 0x4ed8aa4aL, 0x5b9cca4fL, 0x682e6ff3L,
		0x748f82eeL, 0x78a5636fL, 0x84c87814L, 0x8cc70208L,
		0x90befffaL, 0xa4506cebL, 0xbef9a3f7L, 0xc67178f2L
	};

#ifndef RUNTIME_ENDIAN

#ifdef WORDS_BIGENDIAN

#define BYTESWAP(x) (x)
#define BYTESWAP64(x) (x)

#else				/* WORDS_BIGENDIAN */

#define BYTESWAP(x) ((ROTR((x), 8) & 0xff00ff00L) |	\
	(ROTL((x), 8) & 0x00ff00ffL))
#define BYTESWAP64(x) _byteswap64(x)

	static inline uint64_t _byteswap64(uint64_t x)
	{
		uint32_t a = x >> 32;
		uint32_t b = (uint32_t) x;
		return ((uint64_t) BYTESWAP(b) << 32) | (uint64_t) BYTESWAP(a);
	}

#endif				/* WORDS_BIGENDIAN */

#else				/* !RUNTIME_ENDIAN */

	static int littleEndian;

#define BYTESWAP(x) _byteswap(x)
#define BYTESWAP64(x) _byteswap64(x)

#define _BYTESWAP(x) ((ROTR((x), 8) & 0xff00ff00L) |	\
	(ROTL((x), 8) & 0x00ff00ffL))
#define _BYTESWAP64(x) __byteswap64(x)

	static inline uint64_t __byteswap64(uint64_t x)
	{
		uint32_t a = x >> 32;
		uint32_t b = (uint32_t) x;
		return ((uint64_t) _BYTESWAP(b) << 32) | (uint64_t) _BYTESWAP(a);
	}

	static inline uint32_t _byteswap(uint32_t x)
	{
		if (!littleEndian)
			return x;
		else
			return _BYTESWAP(x);
	}

	static inline uint64_t _byteswap64(uint64_t x)
	{
		if (!littleEndian)
			return x;
		else
			return _BYTESWAP64(x);
	}

	static inline void setEndian(void)
	{
		union {
			uint32_t w;
			uint8_t b[4];
		} endian;

		endian.w = 1L;
		littleEndian = endian.b[0] != 0;
	}

#endif				/* !RUNTIME_ENDIAN */

	static const uint8_t padding[64] = {
		0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};

	void sha256_init(sha256_ctx_t * sc)
	{
#ifdef RUNTIME_ENDIAN
		setEndian();
#endif				/* RUNTIME_ENDIAN */

		sc->totalLength = 0LL;
		sc->hash[0] = 0x6a09e667L;
		sc->hash[1] = 0xbb67ae85L;
		sc->hash[2] = 0x3c6ef372L;
		sc->hash[3] = 0xa54ff53aL;
		sc->hash[4] = 0x510e527fL;
		sc->hash[5] = 0x9b05688cL;
		sc->hash[6] = 0x1f83d9abL;
		sc->hash[7] = 0x5be0cd19L;
		sc->bufferLength = 0L;
	}

	static void burnStack(int size)
	{
		char buf[128];

		memset(buf, 0, sizeof(buf));
		size -= sizeof(buf);
		if (size > 0)
			burnStack(size);
	}

	static void SHA256Guts(sha256_ctx_t * sc, const uint32_t * cbuf)
	{
		uint32_t buf[64];
		uint32_t *W, *W2, *W7, *W15, *W16;
		uint32_t a, b, c, d, e, f, g, h;
		uint32_t t1, t2;
		const uint32_t *Kp;
		int i;

		W = buf;

		for (i = 15; i >= 0; i--) {
			*(W++) = BYTESWAP(*cbuf);
			cbuf++;
		}

		W16 = &buf[0];
		W15 = &buf[1];
		W7 = &buf[9];
		W2 = &buf[14];

		for (i = 47; i >= 0; i--) {
			*(W++) = sigma1(*W2) + *(W7++) + sigma0(*W15) + *(W16++);
			W2++;
			W15++;
		}

		a = sc->hash[0];
		b = sc->hash[1];
		c = sc->hash[2];
		d = sc->hash[3];
		e = sc->hash[4];
		f = sc->hash[5];
		g = sc->hash[6];
		h = sc->hash[7];

		Kp = K;
		W = buf;

#ifndef SHA256_UNROLL
#define SHA256_UNROLL 1
#endif				/* !SHA256_UNROLL */

#if SHA256_UNROLL == 1
		for (i = 63; i >= 0; i--)
			DO_ROUND();
#elif SHA256_UNROLL == 2
		for (i = 31; i >= 0; i--) {
			DO_ROUND();
			DO_ROUND();
		}
#elif SHA256_UNROLL == 4
		for (i = 15; i >= 0; i--) {
			DO_ROUND();
			DO_ROUND();
			DO_ROUND();
			DO_ROUND();
		}
#elif SHA256_UNROLL == 8
		for (i = 7; i >= 0; i--) {
			DO_ROUND();
			DO_ROUND();
			DO_ROUND();
			DO_ROUND();
			DO_ROUND();
			DO_ROUND();
			DO_ROUND();
			DO_ROUND();
		}
#elif SHA256_UNROLL == 16
		for (i = 3; i >= 0; i--) {
			DO_ROUND();
			DO_ROUND();
			DO_ROUND();
			DO_ROUND();
			DO_ROUND();
			DO_ROUND();
			DO_ROUND();
			DO_ROUND();
			DO_ROUND();
			DO_ROUND();
			DO_ROUND();
			DO_ROUND();
			DO_ROUND();
			DO_ROUND();
			DO_ROUND();
			DO_ROUND();
		}
#elif SHA256_UNROLL == 32
		for (i = 1; i >= 0; i--) {
			DO_ROUND();
			DO_ROUND();
			DO_ROUND();
			DO_ROUND();
			DO_ROUND();
			DO_ROUND();
			DO_ROUND();
			DO_ROUND();
			DO_ROUND();
			DO_ROUND();
			DO_ROUND();
			DO_ROUND();
			DO_ROUND();
			DO_ROUND();
			DO_ROUND();
			DO_ROUND();
			DO_ROUND();
			DO_ROUND();
			DO_ROUND();
			DO_ROUND();
			DO_ROUND();
			DO_ROUND();
			DO_ROUND();
			DO_ROUND();
			DO_ROUND();
			DO_ROUND();
			DO_ROUND();
			DO_ROUND();
			DO_ROUND();
			DO_ROUND();
			DO_ROUND();
			DO_ROUND();
		}
#elif SHA256_UNROLL == 64
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
		DO_ROUND();
#else
#error "SHA256_UNROLL must be 1, 2, 4, 8, 16, 32, or 64!"
#endif

		sc->hash[0] += a;
		sc->hash[1] += b;
		sc->hash[2] += c;
		sc->hash[3] += d;
		sc->hash[4] += e;
		sc->hash[5] += f;
		sc->hash[6] += g;
		sc->hash[7] += h;
	}

	void sha256_update(sha256_ctx_t * sc, const void *data, uint32_t len)
	{
		uint32_t bufferBytesLeft;
		uint32_t bytesToCopy;
		int needBurn = 0;

		if (sc->bufferLength) 
		{
			bufferBytesLeft = 64L - sc->bufferLength;
			bytesToCopy = bufferBytesLeft;
			if (bytesToCopy > len)
			{
				bytesToCopy = len;
			}
			memcpy(&sc->buffer.bytes[sc->bufferLength], data, bytesToCopy);
			sc->totalLength += bytesToCopy * 8L;
			sc->bufferLength += bytesToCopy;
			data = ((uint8_t *) data) + bytesToCopy;
			len -= bytesToCopy;
			if (sc->bufferLength == 64L) 
			{
				SHA256Guts(sc, sc->buffer.words);
				needBurn = 1;
				sc->bufferLength = 0L;
			}
		}

		while (len > 63L) 
		{
			sc->totalLength += 512L;

			SHA256Guts(sc, (const uint32_t *)data);
			needBurn = 1;

			data = ((uint8_t *) data) + 64L;
			len -= 64L;
		}

		if (len) 
		{
			memcpy(&sc->buffer.bytes[sc->bufferLength], data, len);
			sc->totalLength += len * 8L;
			sc->bufferLength += len;
		}

		if (needBurn)
		{
			burnStack(sizeof(uint32_t[74]) + sizeof(uint32_t *[6]) +  sizeof(int));
		}
	}

	void sha256_finalize(sha256_ctx_t * sc, uint8_t hash[SHA256_HASH_SIZE])
	{
		uint32_t bytesToPad;
		uint64_t lengthPad;
		int i;

		bytesToPad = 120L - sc->bufferLength;
		if (bytesToPad > 64L)
		{
			bytesToPad -= 64L;
		}

		lengthPad = BYTESWAP64(sc->totalLength);

		sha256_update(sc, padding, bytesToPad);
		sha256_update(sc, &lengthPad, 8L);

		if (hash) 
		{
			for (i = 0; i < SHA256_HASH_WORDS; i++) 
			{
				*((uint32_t *) hash) = BYTESWAP(sc->hash[i]);
				hash += 4;
			}
		}
	}


	void computeSHA256(const void *input,uint32_t size,uint8_t destHash[32])
	{
		sha256_ctx_t sc;
		sha256_init(&sc);
		sha256_update(&sc,input,size);
		sha256_finalize(&sc,destHash);
	}

}; // End of the SHA-2556 namespace


// Begin of source to perform Base58 encode/decode
namespace BLOCKCHAIN_BASE58
{

//
//  BigNumber.c
//  cbitcoin
//
//  Created by Matthew Mitchell on 28/04/2012.
//  Copyright (c) 2012 Matthew Mitchell
//
//  This file is part of cbitcoin.
//
//  cbitcoin is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  cbitcoin is distributed in the hope that it will be useful, 
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with cbitcoin.  If not, see <http://www.gnu.org/licenses/>.
//
//

#define MAX_BIG_NUMBER 256	// this code snippet only supports big nubmers up to 256 bytes in length
/**
 @brief Contains byte data with the length of this data to represent a large integer. The byte data is in little-endian which stores the smallest byte first.
 */
class BigNumber
{
public:
	BigNumber(const uint8_t *sourceData,uint32_t len)
	{
		assert(len<MAX_BIG_NUMBER);
		memset(data,0,MAX_BIG_NUMBER);
		if ( sourceData )
		{
			memcpy(data,sourceData,len);
		}
		length = len;
	}
	uint8_t		data[MAX_BIG_NUMBER]; /**< The byte data. Should be little-endian */
	uint32_t	length; /**< The length of this data in bytes */
};

//  Enums

enum BigNumberCompare 
{
	CB_COMPARE_MORE_THAN = 1, 
	CB_COMPARE_EQUAL = 0, 
	CB_COMPARE_LESS_THAN = -1, 
};

/**
 @brief Compares a BigNumber to an 8 bit integer. You can replicate "a op 58" as "BigNumberCompareToUInt8(a, 58) op 0" replacing "op" with a comparison operator.
 @param a The first BigNumber
 @returns The result of the comparison as a BigNumberCompare constant. Returns what a is in relation to b.
 */
BigNumberCompare BigNumberCompareTo58(BigNumber * a);
/**
 @brief Compares two BigNumber. You can replicate "a op b" as "BigNumberCompare(a, b) op 0" replacing "op" with a comparison operator.
 @param a The first BigNumber
 @param b The second BigNumber
 @returns The result of the comparison as a BigNumberCompare constant. Returns what a is in relation to b.
 */
BigNumberCompare BigNumberCompareToBigInt(BigNumber * a, BigNumber * b);
/**
 @brief Calculates the result of an addition of a BigNumber structure by another BigNumber structure and the first BigNumber becomes this new figure. Like "a += b".
 @param a A pointer to the BigNumber
 @param b A pointer to the second BigNumber
 @returns true on success, false on failure.
 */
bool BigNumberEqualsAdditionByBigInt(BigNumber * a, BigNumber * b);
/**
 @brief Calculates the result of a division of a BigNumber structure by 58 and the BigNumber becomes this new figure. Like "a /= 58".
 @param a A pointer to the BigNumber
 @param ans A memory block the same size as the BigNumber data memory block to store temporary data in calculations. Should be set with zeros.
 */
void BigNumberEqualsDivisionBy58(BigNumber * a, uint8_t * ans);
/**
 @brief Calculates the result of a multiplication of a BigNumber structure by an 8 bit integer and the BigNumber becomes this new figure. Like "a *= b".
 @param a A pointer to the BigNumber
 @param b An 8 bit integer
 @returns true on success, false on failure
 */
bool BigNumberEqualsMultiplicationByUInt8(BigNumber * a, uint8_t b);
/**
 @brief Calculates the result of a subtraction of a BigNumber structure with another BigNumber structure and the BigNumber becomes this new figure. Like "a -= b".
 @param a A pointer to a BigNumber
 @param b A pointer to a BigNumber
 */
void BigNumberEqualsSubtractionByBigInt(BigNumber * a, BigNumber * b);
/**
 @brief Calculates the result of a subtraction of a BigNumber structure by an 8 bit integer and the BigNumber becomes this new figure. Like "a -= b".
 @param a A pointer to the BigNumber
 @param b An 8 bit integer
 */
void BigNumberEqualsSubtractionByUInt8(BigNumber * a, uint8_t b);
/**
 @brief Assigns a BigNumber as the exponentiation of an unsigned 8 bit intger with another unsigned 8 bit integer. Like "a^b". Data must be freed.
 @param bi The BigNumber. Preallocate this with at least one byte.
 @param a The base
 @param b The exponent.
 @returns true on success, false on failure.
 */
bool BigNumberFromPowUInt8(BigNumber * bi, uint8_t a, uint8_t b);
/**
 @brief Returns the result of a modulo of a BigNumber structure and 58. Like "a % 58".
 @param a The BigNumber
 @returns The result of the modulo operation as an 8 bit integer.
 */
uint8_t BigNumberModuloWith58(BigNumber * a);
/**
 @brief Normalises a BigNumber so that there are no unnecessary trailing zeros.
 @param a A pointer to the BigNumber
 */
void BigNumberNormalise(BigNumber * a);

BigNumberCompare BigNumberCompareTo58(BigNumber * a)
{
	if(a->length > 1)
		return CB_COMPARE_MORE_THAN;
	if (a->data[0] > 58)
		return CB_COMPARE_MORE_THAN;
	else if (a->data[0] < 58)
		return CB_COMPARE_LESS_THAN;
	return CB_COMPARE_EQUAL;
}

BigNumberCompare BigNumberCompareToBigInt(BigNumber * a, BigNumber * b)
{
	if (a->length > b->length)
		return CB_COMPARE_MORE_THAN;
	else if (a->length < b->length)
		return CB_COMPARE_LESS_THAN;
	for (uint32_t x = a->length; x--;) 
	{
		if (a->data[x] < b->data[x])
			return CB_COMPARE_LESS_THAN;
		else if (a->data[x] > b->data[x])
			return CB_COMPARE_MORE_THAN;
	}
	return CB_COMPARE_EQUAL;
}
bool BigNumberEqualsAdditionByBigInt(BigNumber * a, BigNumber * b)
{
	if (a->length < b->length) 
	{
		// Make certain expansion of data is empty
		memset(a->data + a->length, 0, b->length - a->length);
		a->length = b->length;
	}
	// a->length >= b->length
	bool overflow = 0;
	uint8_t x = 0;
	for (; x < b->length; x++) 
	{
		a->data[x] += b->data[x] + overflow;
		// a->data[x] now equals the result of the addition.
		// The overflow will never go beyond 1. Imagine a->data[x] == 0xff, b->data[x] == 0xff and the overflow is 1, the new overflow is still 1 and a->data[x] is 0xff. Therefore it does work.
		overflow = (a->data[x] < (b->data[x] + overflow))? 1 : 0;
	}
	// Propagate overflow up the whole length of a if necessary
	while (overflow && x < a->length)
		overflow = ! ++a->data[x++]; // Index at x, increment x, increment data, test new value for overflow.
	if (overflow) 
	{ // Add extra byte
		a->length++;
		assert( a->length < MAX_BIG_NUMBER );
		a->data[a->length - 1] = 1;
	}
	return true;
}
void BigNumberEqualsDivisionBy58(BigNumber * a, uint8_t * ans)
{
	if (a->length == 1 && ! a->data[0]) // "a" is zero
		return;
	// base-256 long division.
	uint16_t temp = 0;
	for (uint32_t x = a->length; x--;) 
	{
		temp <<= 8;
		temp |= a->data[x];
		ans[x] = (uint8_t)(temp / 58);
		temp -= ans[x] * 58;
	}
	if (! ans[a->length-1]) // If last byte is zero, adjust length.
		a->length--;
	memmove(a->data, ans, a->length); // Done calculation. Move ans to "a".
}
bool BigNumberEqualsMultiplicationByUInt8(BigNumber * a, uint8_t b)
{
	if (! b) 
	{
		// Mutliplication by zero. "a" becomes zero
		a->length = 1;
		a->data[0] = 0;
		return true;
	}
	if (a->length == 1 && ! a->data[0]) // "a" is zero
		return true;
	// Multiply b by each byte and then add to answer
	uint16_t carry = 0;
	uint8_t x = 0;
	for (; x < a->length; x++) 
	{
		carry = carry + a->data[x] * b; // Allow for overflow onto next byte.
		a->data[x] = (uint8_t)carry;
		carry >>= 8;
	}
	if (carry) 
	{ // If last byte is not zero, adjust length.
		a->length++;
		assert( a->length < MAX_BIG_NUMBER );
		a->data[x] = (uint8_t)carry;
	}
	return true;
}
void BigNumberEqualsSubtractionByBigInt(BigNumber * a, BigNumber * b)
{
	uint8_t x;
	bool carry = 0;
	// This can be made much nicer when using signed arithmetic, 
	// carry and tmp could be merged to be 0 or -1 between rounds.
	for (x = 0; x < b->length; x++) {
		uint16_t tmp = carry + b->data[x];
		carry = a->data[x] < tmp;
		a->data[x] -= (uint8_t)tmp;
	}
	if (carry)
		a->data[x]--;
	BigNumberNormalise(a);
}

void BigNumberEqualsSubtractionByUInt8(BigNumber * a, uint8_t b)
{
	uint8_t carry = b;
	uint8_t x = 0;
	for (; a->data[x] < carry; x++){
		a->data[x] = 255 - carry + a->data[x] + 1;
		carry = 1;
	}
	a->data[x] -= carry;
	BigNumberNormalise(a);
}

bool BigNumberFromPowUInt8(BigNumber * bi, uint8_t a, uint8_t b)
{
	bi->length = 1;
	bi->data[0] = 1;
	for (uint8_t x = 0; x < b; x++) {
		if (! BigNumberEqualsMultiplicationByUInt8(bi, a))
			// ERROR
			return false;
	}
	return true;
}

uint8_t BigNumberModuloWith58(BigNumber * a)
{
	// Use method presented here: http://stackoverflow.com/a/10441333/238411
	uint16_t result = 0; // Prevent overflow in calculations
	for(uint32_t x = a->length - 1;; x--){
		result *= (256 % 58);
		result %= 58;
		result += a->data[x] % 58;
		result %= 58;
		if (! x)
			break;
	}
	return (uint8_t)result;
}

void BigNumberNormalise(BigNumber * a)
{
	while (a->length > 1 && ! a->data[a->length-1])
		a->length--;
}

//                                                  1         2         3         4         5    
//                                        01234567890123456789012345678901234567890123456789012345678
static const char base58Characters[59] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

/**
 @brief Decodes base 58 string into byte data as a BigNumber.
 @param bi The BigNumber which should be preallocated with at least one byte.
 @param str Base 58 string to decode.
 @returns true on success, false on failure.
 */
bool CBDecodeBase58(BigNumber * bi,const char * str)
{
	// ??? Quite likely these functions can be improved
	BigNumber bi2(NULL,1);

	uint32_t slen = (uint32_t)strlen(str);

	for (uint32_t x = slen - 1;; x--)
	{ // Working backwards
		// Get index in alphabet array
		uint8_t alphaIndex = str[x];
		if (alphaIndex != 49)
		{ // If not 1
			if (str[x] < 58)
			{ // Numbers
				alphaIndex -= 49;
			}
			else if (str[x] < 73)
			{ // A-H
				alphaIndex -= 56;
			}
			else if (str[x] < 79)
			{ // J-N
				alphaIndex -= 57;
			}
			else if (str[x] < 91)
			{ // P-Z
				alphaIndex -= 58;
			}
			else if (str[x] < 108)
			{ // a-k
				alphaIndex -= 64;
			}
			else
			{ // m-z
				alphaIndex -= 65;
			}
			if (! BigNumberFromPowUInt8(&bi2, 58, (uint8_t)(slen - 1 - x)))
			{
				// Error occured.
				return false;
			}
			if (! BigNumberEqualsMultiplicationByUInt8(&bi2, alphaIndex))
			{
				// Error occured.
				return false;
			}
			if (! BigNumberEqualsAdditionByBigInt(bi, &bi2))
			{
				// Error occured.
				return false;
			}
		}
		if (!x)
			break;
	}
	// Got BigNumber from base-58 string. Add zeros on end.
	uint8_t zeros = 0;
	for (uint8_t x = 0; x < slen; x++)
	{
		if (str[x] == '1')
		{
			zeros++;
		}
		else
		{
			break;
		}
	}
	if (zeros) 
	{
		bi->length += zeros;
		assert( bi->length < MAX_BIG_NUMBER );
		memset(bi->data + bi->length - zeros, 0, zeros);
	}
	return true;
}

bool CBEncodeBase58(BigNumber * bi,char *str,uint32_t maxStrLen)
{
	if ( maxStrLen < bi->length ) // must be at least twice the size of the binary
	{
		return false;
	}
	uint32_t x = 0;
	// Zeros
	for (uint32_t y = bi->length - 1;; y--)
	{
		if (! bi->data[y])
		{
			str[x] = '1';
			x++;
			if (! y)
				break;
		}
		else
		{
			break;
		}
	}
	uint32_t zeros = x;
	// Make temporary data store
	uint8_t temp[MAX_BIG_NUMBER];
	// Encode
	uint8_t mod;
	size_t size = bi->length;
	for (;BigNumberCompareTo58(bi) >= 0;x++) 
	{
		mod = BigNumberModuloWith58(bi);
		if (bi->length < x + 3) 
		{
			size = x + 3;
			if (size > bi->length) 
			{
				if ( size > maxStrLen )
				{
					return false;
				}
			}
		}
		str[x] = base58Characters[mod];
		BigNumberEqualsSubtractionByUInt8(bi, mod);
		memset(temp, 0, bi->length);
		BigNumberEqualsDivisionBy58(bi, temp);
	}
	str[x] = base58Characters[bi->data[bi->length-1]];
	x++;
	// Reversal
	for (uint8_t y = 0; y < (x-zeros) / 2; y++) 
	{
		char temp = str[y+zeros];
		str[y+zeros] = str[x-y-1];
		str[x-y-1] = temp;
	}
	str[x] = '\0';
	return true;
}




bool encodeBase58(const uint8_t *bigNumber, // The block of memory corresponding to the 'big number'
					   uint32_t length,			 // The number of bytes in the 'big-number'; this will be 25 for a bitcoin address
					   bool littleEndian,		 // True if the input number is in little-endian format (this will be true for a bitcoin address)
					   char *output,			 // The address to store the output string.
					   uint32_t maxStrLen)		 // the maximum length of the output string
{
	// Before passing the hash into the base58 encoder; we need to reverse the byte order.
	uint8_t hash[25];
	if ( littleEndian )
	{
		uint32_t index = length-1;
		for (uint32_t i=0; i<25; i++)
		{
			hash[i] = bigNumber[index];
			index--;
		}
	}
	else
	{
		memcpy(hash,bigNumber,length);
	}
	// We now have the 25 byte public key address in binary; we just need to convert it to ASCII
	// This involves using large integer math to compute the base58 encoding (with check) 
	BigNumber bytes(hash,length);;
	return CBEncodeBase58(&bytes,output,maxStrLen);
}

uint32_t decodeBase58(const char *string,		// The base58 encoded string
					   uint8_t *output,			// The output binary buffer
					   uint32_t maxOutputLength, // The maximum output length of the binary buffer.
					   bool littleEndian) // If the output needs to be in little endian format
{
	uint32_t ret = 0;

	BigNumber bn(NULL,0);
	if ( CBDecodeBase58(&bn,string) && bn.length <= maxOutputLength )
	{
		ret = bn.length;
		if ( littleEndian ) // if the caller wants the number returned in little endian format; then we byte reverse the output
		{
			uint32_t index = bn.length-1;
			for (uint32_t i=0; i<bn.length; i++)
			{
				output[i] = bn.data[index];
				index--;
			}
		}
		else
		{
			memcpy(output,bn.data,bn.length);
		}
	}

	return ret;
}

}; // end of namespace

// ************ Beginning of source to compute bitcoin addresses from public keys, etc.
namespace BLOCKCHAIN_BITCOIN_ADDRESS
{

bool bitcoinPublicKeyToAddress(const uint8_t input[65], // The 65 bytes long ECDSA public key; first byte will always be 0x4 followed by two 32 byte components
							   uint8_t output[25])		// A bitcoin address (in binary( is always 25 bytes long.
{
	bool ret = false;

	if ( input[0] == 0x04)
	{
		uint8_t hash1[32]; // holds the intermediate SHA256 hash computations
		BLOCKCHAIN_SHA256::computeSHA256(input,65,hash1);	// Compute the SHA256 hash of the input public ECSDA signature
		output[0] = 0;	// Store a network byte of 0 (i.e. 'main' network)
		BLOCKCHAIN_RIPEMD160::computeRIPEMD160(hash1,32,&output[1]);	// Compute the RIPEMD160 (20 byte) hash of the SHA256 hash
		BLOCKCHAIN_SHA256::computeSHA256(output,21,hash1);	// Compute the SHA256 hash of the RIPEMD16 hash + the one byte header (for a checksum)
		BLOCKCHAIN_SHA256::computeSHA256(hash1,32,hash1); // now compute the SHA256 hash of the previously computed SHA256 hash (for a checksum)
		output[21] = hash1[0];	// Store the checksum in the last 4 bytes of the public key hash
		output[22] = hash1[1];
		output[23] = hash1[2];
		output[24] = hash1[3];
		ret = true;
	}
	return ret;
}


bool bitcoinPublicKeyToAscii(const uint8_t input[65], // The 65 bytes long ECDSA public key; first byte will always be 0x4 followed by two 32 byte components
							 char *output,				// The output ascii representation.
							 uint32_t maxOutputLen) // convert a binary bitcoin address into ASCII
{
	bool ret = false;

	output[0] = 0;

	uint8_t hash2[25];

	if ( bitcoinPublicKeyToAddress(input,hash2))
	{
		ret = BLOCKCHAIN_BASE58::encodeBase58(hash2,25,true,output,maxOutputLen);
	}
	return ret;
}

bool bitcoinAsciiToAddress(const char *input,uint8_t output[25]) // convert an ASCII bitcoin address into binary.
{
	bool ret = false;
	uint32_t len = BLOCKCHAIN_BASE58::decodeBase58(input,output,25,true);
	if ( len == 25 ) // the output must be *exactly* 25 bytes!
	{
		uint8_t checksum[32];
		BLOCKCHAIN_SHA256::computeSHA256(output,21,checksum);
		BLOCKCHAIN_SHA256::computeSHA256(checksum,32,checksum);
		if ( output[21] == checksum[0] ||
			 output[22] == checksum[1] ||
			 output[23] == checksum[2] ||
			 output[24] == checksum[3] )
		{
			ret = true; // the cheksum matches!
		}
	}
	return ret;
}



void bitcoinRIPEMD160ToAddress(const uint8_t ripeMD160[20],uint8_t output[25])
{
	uint8_t hash1[32]; // holds the intermediate SHA256 hash computations
	output[0] = 0;	// Store a network byte of 0 (i.e. 'main' network)
	memcpy(&output[1],ripeMD160,20); // copy the 20 byte of the public key address
	BLOCKCHAIN_SHA256::computeSHA256(output,21,hash1);	// Compute the SHA256 hash of the RIPEMD16 hash + the one byte header (for a checksum)
	BLOCKCHAIN_SHA256::computeSHA256(hash1,32,hash1); // now compute the SHA256 hash of the previously computed SHA256 hash (for a checksum)
	output[21] = hash1[0];	// Store the checksum in the last 4 bytes of the public key hash
	output[22] = hash1[1];
	output[23] = hash1[2];
	output[24] = hash1[3];
}

bool bitcoinAddressToAscii(const uint8_t address[25],char *output,uint32_t maxOutputLen)
{
	bool ret = false;

	ret = BLOCKCHAIN_BASE58::encodeBase58(address,25,true,output,maxOutputLen);

	return ret;
}

}; // end of namespace


enum ScriptOpcodes
{
	OP_0 			=  0x00,
	OP_PUSHDATA1 	=  0x4c,
	OP_PUSHDATA2 	=  0x4d,
	OP_PUSHDATA4 	=  0x4e,
	OP_1NEGATE 		=  0x4f,
	OP_RESERVED 	=  0x50,
	OP_1 			=  0x51,
	OP_2 			=  0x52,
	OP_3 			=  0x53,
	OP_4 			=  0x54,
	OP_5 			=  0x55,
	OP_6 			=  0x56,
	OP_7 			=  0x57,
	OP_8 			=  0x58,
	OP_9 			=  0x59,
	OP_10 			=  0x5a,
	OP_11 			=  0x5b,
	OP_12 			=  0x5c,
	OP_13 			=  0x5d,
	OP_14 			=  0x5e,
	OP_15 			=  0x5f,
	OP_16 			=  0x60,
	OP_NOP 			=  0x61,
	OP_VER 			=  0x62,
	OP_IF 			=  0x63,
	OP_NOTIF 		=  0x64,
	OP_VERIF 		=  0x65,
	OP_VERNOTIF 	=  0x66,
	OP_ELSE 		=  0x67,
	OP_ENDIF 		=  0x68,
	OP_VERIFY 		=  0x69,
	OP_RETURN 		=  0x6a,
	OP_TOALTSTACK 	=  0x6b,
	OP_FROMALTSTACK =  0x6c,
	OP_2DROP 		=  0x6d,
	OP_2DUP 		=  0x6e,
	OP_3DUP 		=  0x6f,
	OP_2OVER 		=  0x70,
	OP_2ROT 		=  0x71,
	OP_2SWAP 		=  0x72,
	OP_IFDUP 		=  0x73,
	OP_DEPTH 		=  0x74,
	OP_DROP 		=  0x75,
	OP_DUP 			=  0x76,
	OP_NIP 			=  0x77,
	OP_OVER 		=  0x78,
	OP_PICK 		=  0x79,
	OP_ROLL 		=  0x7a,
	OP_ROT 			=  0x7b,
	OP_SWAP 		=  0x7c,
	OP_TUCK 		=  0x7d,
	OP_CAT 			=  0x7e,	// Currently disabled
	OP_SUBSTR 		=  0x7f,	// Currently disabled
	OP_LEFT 		=  0x80,	// Currently disabled
	OP_RIGHT 		=  0x81,	// Currently disabled
	OP_SIZE 		=  0x82,	// Currently disabled
	OP_INVERT 		=  0x83,	// Currently disabled
	OP_AND 			=  0x84,	// Currently disabled
	OP_OR 			=  0x85,	// Currently disabled
	OP_XOR 			=  0x86,	// Currently disabled
	OP_EQUAL 		=  0x87,
	OP_EQUALVERIFY 	=  0x88,
	OP_RESERVED1 	=  0x89,
	OP_RESERVED2 	=  0x8a,
	OP_1ADD 		=  0x8b,
	OP_1SUB 		=  0x8c,
	OP_2MUL 		=  0x8d,	// Currently disabled
	OP_2DIV 		=  0x8e,	// Currently disabled
	OP_NEGATE 		=  0x8f,
	OP_ABS 			=  0x90,
	OP_NOT 			=  0x91,
	OP_0NOTEQUAL 	=  0x92,
	OP_ADD 			=  0x93,
	OP_SUB 			=  0x94,
	OP_MUL 			=  0x95,	// Currently disabled
	OP_DIV 			=  0x96,	// Currently disabled
	OP_MOD 			=  0x97,	// Currently disabled
	OP_LSHIFT 		=  0x98,	// Currently disabled
	OP_RSHIFT 		=  0x99,	// Currently disabled
	OP_BOOLAND 		=  0x9a,
	OP_BOOLOR 		=  0x9b,
	OP_NUMEQUAL 	=  0x9c,
	OP_NUMEQUALVERIFY =  0x9d,
	OP_NUMNOTEQUAL 	=  0x9e,
	OP_LESSTHAN 	=  0x9f,
	OP_GREATERTHAN 	=  0xa0,
	OP_LESSTHANOREQUAL =  0xa1,
	OP_GREATERTHANOREQUAL =  0xa2,
	OP_MIN 			=  0xa3,
	OP_MAX 			=  0xa4,
	OP_WITHIN 		=  0xa5,
	OP_RIPEMD160 	=  0xa6,
	OP_SHA1 		=  0xa7,
	OP_SHA256		=  0xa8,
	OP_HASH160 		=  0xa9,
	OP_HASH256 		=  0xaa,
	OP_CODESEPARATOR =  0xab,
	OP_CHECKSIG 	=  0xac,
	OP_CHECKSIGVERIFY =  0xad,
	OP_CHECKMULTISIG =  0xae,
	OP_CHECKMULTISIGVERIFY = 0xaf,
	OP_NOP1 		=  0xb0,
	OP_NOP2 		=  0xb1,
	OP_NOP3 		=  0xb2,
	OP_NOP4 		=  0xb3,
	OP_NOP5 		=  0xb4,
	OP_NOP6 		=  0xb5,
	OP_NOP7 		=  0xb6,
	OP_NOP8 		=  0xb7,
	OP_NOP9 		=  0xb8,
	OP_NOP10 		=  0xb9,
	OP_SMALLINTEGER =  0xfa,
	OP_PUBKEYS 		=  0xfb,
	OP_PUBKEYHASH 	=  0xfd,
	OP_PUBKEY 		=  0xfe,
	OP_INVALIDOPCODE =  0xff
};

#define MAGIC_ID 0xD9B4BEF9
#define ONE_BTC 100000000
#define ONE_MBTC (ONE_BTC/1000)

#define MAX_BLOCK_FILES	512	// As of July 6, 2013 there are only about 70 .dat files; so it will be a long time before this overflows

// These defines set the limits this parser expects to ever encounter on the blockchain data stream.
// In a debug build there are asserts to make sure these limits are never exceeded.
// These limits work for the blockchain current as of July 1, 2013.
// The limits can be revised when and if necessary.
#define MAX_BLOCK_SIZE (1024*1024)*10	// never expect to have a block larger than 10mb
#define MAX_BLOCK_TRANSACTION 8192		// never expect more than 8192 transactions per block.
#define MAX_BLOCK_INPUTS 32768			// never expect more than 32768 total inputs
#define MAX_BLOCK_OUTPUTS 32768			// never expect more than 32768 total outputs

#define MAX_REASONABLE_SCRIPT_LENGTH (1024*8) // would never expect any script to be more than 8k in size; that would be very unusual!
#define MAX_REASONABLE_INPUTS 4096				// really can't imagine any transaction ever having more than 4096 inputs
#define MAX_REASONABLE_OUTPUTS 4096				// really can't imagine any transaction ever having more than 4096 outputs

//********************************************
//********************************************

class BlockImpl : public BlockChain::Block
{
public:

	// Read one byte from the block-chain input stream.
	inline uint8_t readU8(void)
	{
		assert( (mBlockRead+sizeof(uint8_t)) <= mBlockEnd );
		uint8_t ret = *(uint8_t *)mBlockRead;
		mBlockRead+=sizeof(uint8_t);
		return ret;
	}

	// Read two bytes from the block-chain input stream.
	inline uint16_t readU16(void)
	{
		assert( (mBlockRead+sizeof(uint16_t)) <= mBlockEnd );
		uint16_t ret = *(uint16_t *)mBlockRead;
		mBlockRead+=sizeof(uint16_t);
		return ret;
	}

	// Read four bytes from the block-chain input stream.
	inline uint32_t readU32(void)
	{
		assert( (mBlockRead+sizeof(uint32_t)) <= mBlockEnd );
		uint32_t ret = *(uint32_t *)mBlockRead;
		mBlockRead+=sizeof(uint32_t);
		return ret;
	}

	// Read eight bytes from the block-chain input stream.
	inline uint64_t readU64(void)
	{
		assert( (mBlockRead+sizeof(uint64_t)) <= mBlockEnd );
		uint64_t ret = *(uint64_t *)mBlockRead;
		mBlockRead+=sizeof(uint64_t);
		return ret;
	}

	// Return the current stream pointer representing a 32byte hash and advance the read pointer accordingly
	inline const uint8_t *readHash(void)
	{
		const uint8_t *ret = mBlockRead;
		assert( (mBlockRead+32) <= mBlockEnd );
		mBlockRead+=32;
		return ret;
	}

	// reads a variable length integer.
	// See the documentation from here:  https://en.bitcoin.it/wiki/Protocol_specification#Variable_length_integer
	inline uint32_t readVariableLengthInteger(void)
	{
		uint32_t ret = 0;

		uint8_t v = readU8();
		if ( v < 0xFD ) // If it's less than 0xFD use this value as the unsigned integer
		{
			ret = (uint32_t)v;
		}
		else
		{
			uint16_t v = readU16();
			if ( v < 0xFFFF )
			{
				ret = (uint32_t)v;
			}
			else
			{
				uint32_t v = readU32();
				if ( v < 0xFFFFFFFF )
				{
					ret = (uint32_t)v;
				}
				else
				{
					assert(0); // never expect to actually encounter a 64bit integer in the block-chain stream; it's outside of any reasonable expected value
					uint64_t v = readU64();
					ret = (uint32_t)v;
				}
			}
		}
		return ret;
	}

	// Get the current read buffer address and advance the stream buffer by this length; used to get the address of input/output scripts
	inline const uint8_t * getReadBufferAdvance(uint32_t readLength)
	{
		const uint8_t *ret = mBlockRead;
		mBlockRead+=readLength;
		assert( mBlockRead <= mBlockEnd );
		return ret;
	}
	
	// Read a transaction input
	bool readInput(BlockChain::BlockInput &input)
	{
		bool ret = true;

		input.transactionHash = readHash();	// read the transaction hash
		input.transactionIndex = readU32();	// read the transaction index
		input.responseScriptLength = readVariableLengthInteger();	// read the length of the script
		assert( input.responseScriptLength < MAX_REASONABLE_SCRIPT_LENGTH );
		if ( input.responseScriptLength < MAX_REASONABLE_SCRIPT_LENGTH )
		{
			input.responseScript = input.responseScriptLength ? getReadBufferAdvance(input.responseScriptLength) : NULL;	// get the script buffer pointer; and advance the read location
			input.sequenceNumber = readU32();
		}
		else
		{
			ret = false;
		}
		return ret;
	}

	// Read an output block
	bool readOutput(BlockChain::BlockOutput &output)
	{
		bool ret = true;

		output.value = readU64();	// Read the value of the transaction
		output.publicKey = NULL;
		blockReward+=output.value;
		output.challengeScriptLength = readVariableLengthInteger();
		assert ( output.challengeScriptLength < MAX_REASONABLE_SCRIPT_LENGTH );

		if ( output.challengeScriptLength < MAX_REASONABLE_SCRIPT_LENGTH )
		{
			output.challengeScript = output.challengeScriptLength ? getReadBufferAdvance(output.challengeScriptLength) : NULL; // get the script buffer pointer and advance the read location
			if ( output.challengeScriptLength == 67 && output.challengeScript[0] == 65  && output.challengeScript[66]== OP_CHECKSIG )
			{
				output.publicKey = output.challengeScript+1;
				output.isRipeMD160 = false;
			}
			else if ( output.challengeScriptLength == 66 && output.challengeScript[65]== OP_CHECKSIG )
			{
				output.publicKey = output.challengeScript;
				output.isRipeMD160 = false;
			}
			else if ( output.challengeScriptLength >= 25 && 
					  output.challengeScript[0] == OP_DUP &&
					  output.challengeScript[1] == OP_HASH160 && 
					  output.challengeScript[2] == 20 )
			{
				output.publicKey = output.challengeScript+3;
				output.isRipeMD160 = true;
			}
			else if ( output.challengeScriptLength == 5 && 
					  output.challengeScript[0] == OP_DUP &&
					  output.challengeScript[1] == OP_HASH160 &&
					  output.challengeScript[2] == OP_0 && 
					  output.challengeScript[3] == OP_EQUALVERIFY &&
					  output.challengeScript[4] == OP_CHECKSIG )
			{
				printf("WARNING: Unusual but expected output script. Block %s : Transaction: %s : OutputIndex: %s\r\n", formatNumber(gBlockIndex), formatNumber(gTransactionIndex), formatNumber(gOutputIndex) );
				gIsWarning = true;
			}
			else
			{
				// Ok..we are going to scan for this pattern.. OP_DUP, OP_HASH160, 0x14 then exactly 20 bytes after 0x88,0xAC
				// 25...
				if ( output.challengeScriptLength > 25 )
				{
					uint32_t endIndex = output.challengeScriptLength-25;

					for (uint32_t i=0; i<endIndex; i++)
					{
						const uint8_t *scan = &output.challengeScript[i];
						if ( scan[0] == OP_DUP &&
							 scan[1] == OP_HASH160 &&
							 scan[2] == 20 &&
							 scan[23] == OP_EQUALVERIFY &&
							 scan[24] == OP_CHECKSIG )
						{
							output.publicKey = &scan[3];
							output.isRipeMD160 = true;
							printf("WARNING: Unusual output script. Block %s : Transaction: %s : OutputIndex: %s\r\n", formatNumber(gBlockIndex), formatNumber(gTransactionIndex), formatNumber(gOutputIndex) );
							gIsWarning = true;
							break;
						}
					}
				}
				if ( !output.publicKey )
				{
					if ( output.challengeScriptLength >= 66 && output.challengeScript[output.challengeScriptLength-1] == OP_CHECKSIG )
					{
						printf("WARNING: Failed to decode public key in output script. Block %s : Transaction: %s : OutputIndex: %s\r\n", formatNumber(gBlockIndex), formatNumber(gTransactionIndex), formatNumber(gOutputIndex) );
						gIsWarning = true;
					}
				}
			}
		}
		else
		{
			ret = false;
		}

		return ret;
	}

	// Read a single transaction
	bool readTransation(BlockChain::BlockTransaction &transaction,uint32_t &transactionIndex,uint32_t tindex)
	{
		bool ret = false;

		const uint8_t *transactionBegin = mBlockRead;

		transaction.transactionVersionNumber = readU32(); // read the transaction version number; always expect it to be 1
		if ( transaction.transactionVersionNumber == 1 || transaction.transactionVersionNumber == 2 )
		{
		}
		else
		{
			gIsWarning = true;
			printf("Encountered unusual and unexpected transaction version number of [%d] for transaction #%d\r\n", transaction.transactionVersionNumber, tindex );
		}
		transaction.inputCount = readVariableLengthInteger();
		assert( transaction.inputCount < MAX_REASONABLE_INPUTS );
		transaction.inputs = &mInputs[totalInputCount];
		totalInputCount+=transaction.inputCount;
		assert( totalInputCount < MAX_BLOCK_INPUTS );
		if ( totalInputCount < MAX_BLOCK_INPUTS )
		{
			for (uint32_t i=0; i<transaction.inputCount; i++)
			{
				BlockChain::BlockInput &input = transaction.inputs[i];
				ret = readInput(input);	// read the input 
				if ( !ret )
				{
					break;
				}
			}
		}
		else
		{
			ret = false;
		}
		if ( ret )
		{
			transaction.outputCount = readVariableLengthInteger();
			assert( transaction.outputCount < MAX_REASONABLE_OUTPUTS );
			transaction.outputs = &mOutputs[totalOutputCount];
			totalOutputCount+=transaction.outputCount;
			assert( totalOutputCount < MAX_BLOCK_OUTPUTS );
			if ( totalOutputCount < MAX_BLOCK_OUTPUTS )
			{
				for (uint32_t i=0; i<transaction.outputCount; i++)
				{
					gOutputIndex = i;
					BlockChain::BlockOutput &output = transaction.outputs[i];
					ret = readOutput(output);
					if ( !ret )
					{
						break;
					}
				}

				transaction.lockTime = readU32();

				if ( ret )
				{
					transaction.transactionLength = (uint32_t)(mBlockRead - transactionBegin);
					transaction.fileIndex = fileIndex;
					transaction.fileOffset = fileOffset + (uint32_t)(transactionBegin-mBlockData);
					transaction.transactionIndex = transactionIndex;
					transactionIndex++;
					BLOCKCHAIN_SHA256::computeSHA256(transactionBegin,transaction.transactionLength,transaction.transactionHash);
					BLOCKCHAIN_SHA256::computeSHA256(transaction.transactionHash,32,transaction.transactionHash);
				}

			}
		}
		return ret;
	}

	// @see this link for detailed documentation:
	//
	// http://james.lab6.com/2012/01/12/bitcoin-285-bytes-that-changed-the-world/
	//
	// read a single block from the block chain into memory
	// Here is how a block is read.
	//
	// Step #1 : We read the block format version
	// Step #2 : We read the hash of the previous block
	// Step #3 : We read the merkle root hash
	// Step #4 : We read the block time stamp
	// Step #5 : We read a 'bits' field; internal use defined by the bitcoin software
	// Step #6 : We read the 'nonce' value; a randum number generated during the mining process.
	// Step #7 : We read the transaction count
	// Step #8 : For/Each Transaction
	//          : (a) We read the transaction version number.
	//          : (b) We read the number of inputs.
	//Step #8a : For/Each input
	//			: (a) Read the hash of the input transaction
	//			: (b) Read the input transaction index
	//			: (c) Read the response script length
	//			: (d) Read the response script data; parsed using the bitcoin scripting system; a little virtual machine.
	//			: Read the sequence number.
	//			: Read the number of outputs
	//Step #8b : For/Each Output
	//			: (a) Read the value of the output in BTC fixed decimal; see docs.
	//			: (b) Read the length of the challenge script.
	//			: (c) Read the challenge script
	//Step #9 Read the LockTime; a value currently always hard-coded to zero
	bool processBlockData(const void *blockData,uint32_t blockLength,uint32_t &transactionIndex)
	{
		bool ret = true;
		mBlockData = (const uint8_t *)blockData;
		mBlockRead = mBlockData;	// Set the block-read scan pointer.
		mBlockEnd = &mBlockData[blockLength]; // Mark the end of block pointer
		blockFormatVersion = readU32();	// Read the format version
		previousBlockHash = readHash();  // get the address of the hash
		merkleRoot = readHash();	// Get the address of the merkle root hash
		timeStamp = readU32();	// Get the timestamp
		bits = readU32();	// Get the bits field
		nonce = readU32();	// Get the 'nonce' random number.
		transactionCount = readVariableLengthInteger();	// Read the number of transactions
		if ( transactionCount < MAX_BLOCK_TRANSACTION )
		{
			transactions = mTransactions;	// Assign the transactions buffer pointer
			for (uint32_t i=0; i<transactionCount; i++)
			{
				gTransactionIndex = i;
				BlockChain::BlockTransaction &b = transactions[i];
				if ( !readTransation(b,transactionIndex,i) )	// Read the transaction; if it failed; then abort processing the block chain
				{
					ret = false;
					break;
				}
			}
		}
		return ret;
	}

	const BlockChain::BlockTransaction *processTransactionData(const void *transactionData,uint32_t transactionLength)
	{
		uint32_t transactionIndex=0;
		BlockChain::BlockTransaction *ret = &mTransactions[0];
		mBlockData = (const uint8_t *)transactionData;
		mBlockRead = mBlockData;	// Set the block-read scan pointer.
		mBlockEnd = &mBlockData[transactionLength]; // Mark the end of block pointer
		if ( !readTransation(*ret,transactionIndex,0) )	// Read the transaction; if it failed; then abort processing the block chain
		{
			ret = NULL;
		}
		return ret;
	}


	const uint8_t					*mBlockRead;				// The current read buffer address in the block
	const uint8_t					*mBlockEnd;					// The EOF marker for the block
	const uint8_t					*mBlockData;
	BlockChain::BlockTransaction	mTransactions[MAX_BLOCK_TRANSACTION];	// Holds the array of transactions
	BlockChain::BlockInput			mInputs[MAX_BLOCK_INPUTS];	// The input arrays
	BlockChain::BlockOutput			mOutputs[MAX_BLOCK_OUTPUTS]; // The output arrays

};

class Transaction;

// Contains a hash of just the 20 byte RIPEMD160 key; does not have the header or footer; which can be calculated.
class BitcoinAddress 
{
public:
	BitcoinAddress(void)
	{
		mWord0 = 0;
		mWord1 = 0;
		mWord2 = 0;
		mTransactionCount = 0;
		mTransactionIndex = 0xFFFFFFFF;
		mTransactions = NULL;
		mTotalReceived = 0;
		mTotalSent = 0;
		mLastInputTime = 0;
		mLastOutputTime = 0;
		mFirstOutputTime = 0;
		mInputCount = 0;
		mOutputCount = 0;
	}

	BitcoinAddress(const uint8_t address[20]) 
	{
		mWord0 = *(const uint64_t *)(address);
		mWord1 = *(const uint64_t *)(address+8);
		mWord2 = *(const uint32_t *)(address+16);
		mTransactionCount = 0;
		mTransactionIndex = 0xFFFFFFFF;
		mTransactions = NULL;
		mTotalReceived = 0;
		mTotalSent = 0;
		mLastInputTime = 0;
		mLastOutputTime = 0;
		mFirstOutputTime = 0;
		mInputCount = 0;
		mOutputCount = 0;
	}

	bool operator==(const BitcoinAddress &a) const
	{
		return mWord0 == a.mWord0 && mWord1 == a.mWord1 && mWord2 == a.mWord2;
	}

	uint32_t getHash(void) const
	{
		const uint32_t *h = (const uint32_t *)&mWord0;
		return h[0] ^ h[1] ^ h[2] ^ h[3] ^ h[4];
	}

	uint32_t getLastUsedTime(void) const
	{
		uint32_t lastUsed = mLastInputTime; // the last time we sent money (not received because anyone can send us money).
		if ( mLastInputTime == 0 ) // if it has never had a spend.. then we use the time of first receive..
		{
			lastUsed = mFirstOutputTime;
		}
		return lastUsed;
	}

	uint32_t getDaysSinceLastUsed(void) const
	{
		time_t currentTime;
		time(&currentTime); // get the current time.
		uint32_t lastUsed = mLastInputTime; // the last time we sent money (not received because anyone can send us money).
		if ( mLastInputTime == 0 ) // if it has never had a spend.. then we use the time of first receive..
		{
			lastUsed = mFirstOutputTime;
		}
		double seconds = difftime(currentTime,time_t(lastUsed));
		double minutes = seconds/60;
		double hours = minutes/60;
		uint32_t days = (uint32_t) (hours/24);
		return days;
	}

	uint32_t getInputCount(void) const
	{
		return mInputCount;
	}

	uint32_t getOutputCount(void)
	{
		return mOutputCount;
	}

	uint64_t getBalance(void) const
	{
		return mTotalReceived - mTotalSent;
	}

	uint64_t	mWord0; // 8
	uint64_t	mWord1; // 16
	uint32_t	mWord2; // 20
	uint32_t	mLastInputTime;
	uint32_t	mLastOutputTime;
	uint32_t	mFirstOutputTime;
	uint64_t	mTotalSent;
	uint64_t	mTotalReceived;
	uint32_t	mInputCount;
	uint32_t	mOutputCount;
	uint32_t	mTransactionIndex;
	uint32_t	mTransactionCount;
	Transaction	**mTransactions;	// The array of transactions associated with this bitcoin-address as either inputs or outputs or (sometimes) both.
};



#pragma warning(push)
#pragma warning(disable:4100)


class TransactionOutput
{
public:
	TransactionOutput(void)
	{
		mValue = 0;
		mAddress = 0;
	}
	uint64_t	mValue;		// value of the output.
	uint32_t	mAddress;	// address of the output. 
};

class TransactionInput
{
public:
	TransactionInput(void)
	{
		mOutput = NULL;
	}
	TransactionOutput	*mOutput;	// All inputs are the result of a previous output or block-reward mining fee (otherwise known as 'coinbase')
};

class Transaction
{
public:
	Transaction(void)
	{
		mInputCount = 0;
		mOutputCount = 0;
		mInputs = 0;
		mOutputCount = 0;
	}
	uint32_t			mBlock;
	uint32_t			mTime;
	uint32_t			mInputCount;
	uint32_t			mOutputCount;
	TransactionInput	*mInputs;
	TransactionOutput	*mOutputs;
};


typedef SimpleHash< BitcoinAddress, 4194304, MAX_BITCOIN_ADDRESSES > BitcoinAddressHashMap;

enum AgeMarker
{
	AM_ONE_DAY,
	AM_ONE_WEEK,
	AM_ONE_MONTH,
	AM_THREE_MONTHS,
	AM_SIX_MONTHS,
	AM_ONE_YEAR,
	AM_TWO_YEARS,
	AM_THREE_YEARS,
	AM_FOUR_YEARS,
	AM_FIVE_YEARS,
	AM_LAST
};

const char * getAgeString(AgeMarker m)
{
	const char *ret = "UNKNOWN";
	switch ( m )
	{
		case AM_ONE_DAY: ret = "One Day"; break;
		case AM_ONE_WEEK: ret = "One Week"; break;
		case AM_ONE_MONTH: ret = "One Month"; break;
		case AM_THREE_MONTHS: ret = "1-3 Months"; break;
		case AM_SIX_MONTHS: ret = "3-6 Months"; break;
		case AM_ONE_YEAR: ret = "Six Months to One Year"; break;
		case AM_TWO_YEARS: ret = "One to Two Years"; break;
		case AM_THREE_YEARS: ret = "Two to Three Years"; break;
		case AM_FOUR_YEARS: ret = "Three to Four Years"; break;
		case AM_FIVE_YEARS: ret = "Four to Fix Years"; break;
	}
	return ret;
}

class AgeStat
{
public:
	AgeStat(void)
	{
		mTotalValue = 0;
		mCount = 0;
	}
	void addValue(uint64_t b)
	{
		mTotalValue+=b;
		if ( b >= ONE_BTC )
		{
			mCount++;
		}
	}
	uint64_t	mTotalValue;
	uint32_t	mCount;
};

enum StatSize
{
	SS_ZERO,			// addresses with a balance of exactly zero
	SS_ONE_MBTC,		// addresses with a balance of one MBTC or less but not zero
	SS_FIVE_MBTC,

	SS_TEN_MBTC,		// addresses with a balance of ten MBTC
	SS_FIFTY_MBTC,

	SS_HUNDRED_MBTC,		// addresses with 100 mbtc
	SS_FIVE_HUNDRED_MBTC,

	SS_ONE_BTC,		// addresses with one BTC
	SS_FIVE_BTC,		// address with between 2 and 5 btc

	SS_TEN_BTC,	// addresses with ten BTC
	SS_FIFTY_BTC,

	SS_HUNDRED_BTC,
	SS_FIVE_HUNDRED_BTC,

	SS_ONE_THOUSAND_BTC,
	SS_FIVE_THOUSAND_BTC,

	SS_TEN_THOUSAND_BTC,
	SS_FIFTY_THOUSAND_BTC,
	SS_HUNDRED_THOUSAND_BTC,
	SS_MAX_BTC,
	SS_COUNT,
};

#define MAX_DAYS 2000
#define DAYS_PER 7
#define MAX_DAY_INDEX (MAX_DAYS/DAYS_PER)

class StatValue
{
public:
	StatValue(void)
	{
		mCount  = 0;
		mValue = 0;
	}
	uint32_t	mCount;
	uint64_t	mValue;
};

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

class StatRow
{
public:
	StatRow(void)
	{
		mTime = 0;
		mCount = 0;
		mValue = 0;
		mZombieTotal = 0;
		mZombieCount = 0;
		mAddressCount = 0;
		mAddresses = NULL;
		mNewAddressCount = 0;
		mDeleteAddressCount = 0;
		mChangeAddressCount = 0;
		mSameAddressCount = 0;
		mRiseFromDeadCount = 0;
		mRiseFromDeadAmount = 0;
		mNewAddresses = NULL;
		mChangedAddresses = NULL;
		mDeletedAddresses = NULL;
	}

	~StatRow(void)
	{
		delete []mAddresses;
		delete []mNewAddresses;
		delete []mChangedAddresses;
		delete []mDeletedAddresses;
	}
	uint64_t	mZombieTotal;
	uint32_t	mZombieCount;
	uint32_t	mTime;
	uint32_t	mCount;
	uint64_t	mValue;
	StatValue	mStats[SS_COUNT];

	uint32_t	mAddressCount;		// number of addreses recorded
	StatAddress	*mAddresses;
	
	uint32_t	mNewAddressCount;
	uint32_t	mDeleteAddressCount;
	uint32_t	mChangeAddressCount;
	uint32_t	mSameAddressCount;
	uint32_t	mRiseFromDeadCount;
	uint32_t	mRiseFromDeadAmount;

	StatAddress	*mNewAddresses;
	StatAddress	*mChangedAddresses;
	uint32_t	*mDeletedAddresses;
};

#define MAX_STAT_COUNT (365*6) // reserve room for up to 6 years of 365 days entries...

class QuickSortPointers
{
public:
	void qsort(void **base,int32_t num); // perform the qsort.
protected:
	// -1 less, 0 equal, +1 greater.
	virtual int32_t compare(void **p1,void **p2) = 0;
private:
	void inline swap(char **a,char **b);
};

void QuickSortPointers::swap(char **a,char **b)
{
	char *tmp;

	if ( a != b )
	{
		tmp = *a;
		*a++ = *b;
		*b++ = tmp;
	}
}


void QuickSortPointers::qsort(void **b,int32_t num)
{
	char *lo,*hi;
	char *mid;
	char *bottom, *top;
	int32_t size;
	char *lostk[30], *histk[30];
	int32_t stkptr;
	char **base = (char **)b;

	if (num < 2 ) return;

	stkptr = 0;

	lo = (char *)base;
	hi = (char *)base + sizeof(char **) * (num-1);

nextone:

	size = (int32_t)(hi - lo) / sizeof(char**) + 1;

	mid = lo + (size / 2) * sizeof(char **);
	swap((char **)mid,(char **)lo);
	bottom = lo;
	top = hi + sizeof(char **);

	for (;;)
	{
		do
		{
			bottom += sizeof(char **);
		} while (bottom <= hi && compare((void **)bottom,(void **)lo) <= 0);

		do
		{
			top -= sizeof(char **);
		} while (top > lo && compare((void **)top,(void **)lo) >= 0);

		if (top < bottom) break;

		swap((char **)bottom,(char **)top);

	}

	swap((char **)lo,(char **)top);

	if ( top - 1 - lo >= hi - bottom )
	{
		if (lo + sizeof(char **) < top)
		{
			lostk[stkptr] = lo;
			histk[stkptr] = top - sizeof(char **);
			stkptr++;
		}
		if (bottom < hi)
		{
			lo = bottom;
			goto nextone;
		}
	}
	else
	{
		if ( bottom < hi )
		{
			lostk[stkptr] = bottom;
			histk[stkptr] = hi;
			stkptr++;
		}
		if (lo + sizeof(char **) < top)
		{
			hi = top - sizeof(char **);
			goto nextone; 					/* do small recursion */
		}
	}

	stkptr--;

	if (stkptr >= 0)
	{
		lo = lostk[stkptr];
		hi = histk[stkptr];
		goto nextone;
	}
	return;
}

class SortByBalance : public QuickSortPointers
{
public:
	SortByBalance(BitcoinAddress **addresses,uint32_t count)
	{
		QuickSortPointers::qsort((void **)addresses,(int32_t)count);
	}

	// -1 less, 0 equal, +1 greater.
	virtual int32_t compare(void **p1,void **p2) 
	{
		BitcoinAddress **ba1 = (BitcoinAddress **)p1;
		BitcoinAddress **ba2 = (BitcoinAddress **)p2;
		BitcoinAddress *a1 = *ba1;
		BitcoinAddress *a2 = *ba2;
		uint64_t balance1 = a1->mTotalReceived-a1->mTotalSent;
		uint64_t balance2 = a2->mTotalReceived-a2->mTotalSent;
		if ( balance1 == balance2 ) return 0;
		return balance1 > balance2 ? -1 : 1;
	}
};

class SortByAge : public QuickSortPointers
{
public:
	SortByAge(BitcoinAddress **addresses,uint32_t count)
	{
		QuickSortPointers::qsort((void **)addresses,(int32_t)count);
	}

	// -1 less, 0 equal, +1 greater.
	virtual int32_t compare(void **p1,void **p2) 
	{
		BitcoinAddress **ba1 = (BitcoinAddress **)p1;
		BitcoinAddress **ba2 = (BitcoinAddress **)p2;
		BitcoinAddress *a1 = *ba1;
		BitcoinAddress *a2 = *ba2;
		uint32_t age1 = a1->getDaysSinceLastUsed();
		uint32_t age2 = a2->getDaysSinceLastUsed();
		if ( age1 == age2 ) return 0;
		return age1 > age2 ? -1 : 1;
	}

};

class BitcoinTransactionFactory
{
public:
	BitcoinTransactionFactory(void)
	{
		mTransactionReferences = NULL;
		mTransactions = NULL;
		mInputs = NULL;
		mOutputs = NULL;
		mBlocks = NULL;
		mTransactionCount = 0;
		mTotalInputCount = 0;
		mTotalOutputCount = 0;
		mBlockCount = 0;
		mStatCount = 0;

		mStatLimits[SS_ZERO] = 0;
		mStatLimits[SS_ONE_MBTC] = ONE_MBTC;
		mStatLimits[SS_FIVE_MBTC] = ONE_MBTC*5;

		mStatLimits[SS_TEN_MBTC] = ONE_MBTC*10;
		mStatLimits[SS_FIFTY_MBTC] = ONE_MBTC*50;

		mStatLimits[SS_HUNDRED_MBTC] = ONE_MBTC*100;
		mStatLimits[SS_FIVE_HUNDRED_MBTC] = ONE_MBTC*500;


		mStatLimits[SS_ONE_BTC] = ONE_BTC;
		mStatLimits[SS_FIVE_BTC] = ONE_BTC*5;

		mStatLimits[SS_TEN_BTC] = ONE_BTC*10;
		mStatLimits[SS_FIFTY_BTC] = (uint64_t)ONE_BTC*(uint64_t)50;

		mStatLimits[SS_HUNDRED_BTC] = (uint64_t)ONE_BTC*(uint64_t)100;
		mStatLimits[SS_FIVE_HUNDRED_BTC] = (uint64_t)ONE_BTC*(uint64_t)500;

		mStatLimits[SS_ONE_THOUSAND_BTC] = (uint64_t)ONE_BTC*(uint64_t)1000;
		mStatLimits[SS_FIVE_THOUSAND_BTC] = (uint64_t)ONE_BTC*(uint64_t)5000;

		mStatLimits[SS_TEN_THOUSAND_BTC] = (uint64_t)ONE_BTC*(uint64_t)10000;
		mStatLimits[SS_FIFTY_THOUSAND_BTC] = (uint64_t)ONE_BTC*(uint64_t)50000;
		mStatLimits[SS_HUNDRED_THOUSAND_BTC] = (uint64_t)ONE_BTC*(uint64_t)100000;
		mStatLimits[SS_MAX_BTC] = (uint64_t)ONE_BTC*(uint64_t)21000000;


		mStatLabel[SS_ZERO] = "ZERO";
		mStatLabel[SS_ONE_MBTC] = "<1MBTC";
		mStatLabel[SS_FIVE_MBTC] = "<5MBTC";
		mStatLabel[SS_TEN_MBTC] = "<10MBTC";
		mStatLabel[SS_FIFTY_MBTC] = "<50MBTC";
		mStatLabel[SS_HUNDRED_MBTC] = "<100MBTC";
		mStatLabel[SS_FIVE_HUNDRED_MBTC] = "<500MBTC";
		mStatLabel[SS_ONE_BTC] = "<1BTC";
		mStatLabel[SS_FIVE_BTC] = "<5BTC";
		mStatLabel[SS_TEN_BTC] = "<10BTC";
		mStatLabel[SS_FIFTY_BTC] = "<50BTC";
		mStatLabel[SS_HUNDRED_BTC] = "<100BTC";
		mStatLabel[SS_FIVE_HUNDRED_BTC] = "<500BTC";
		mStatLabel[SS_ONE_THOUSAND_BTC] = "<1KBTC";
		mStatLabel[SS_FIVE_THOUSAND_BTC] = "<5KBTC";
		mStatLabel[SS_TEN_THOUSAND_BTC] = "<10KBTC";
		mStatLabel[SS_FIFTY_THOUSAND_BTC] = "<50KBTC";
		mStatLabel[SS_HUNDRED_THOUSAND_BTC] = "<100KBTC";
		mStatLabel[SS_MAX_BTC] = ">100KBTC";

	}

	~BitcoinTransactionFactory(void)
	{
		delete []mBlocks;
		delete []mTransactions;
		delete []mInputs;
		delete []mOutputs;
		delete []mTransactionReferences;
	}

	void init(void)
	{
		if ( mTransactions == NULL )
		{
			mTransactions = new Transaction[MAX_TOTAL_TRANSACTIONS];
			mInputs = new TransactionInput[MAX_TOTAL_INPUTS];
			mOutputs = new TransactionOutput[MAX_TOTAL_OUTPUTS];
			mBlocks = new Transaction *[MAX_TOTAL_BLOCKS];
		}
	}

	void markBlock(Transaction *t)
	{
		assert( mBlockCount < MAX_TOTAL_BLOCKS );
		if ( mBlockCount < MAX_TOTAL_BLOCKS )
		{
			mBlocks[mBlockCount] = t;
			mBlockCount++;
		}
	}

	Transaction * getBlock(uint32_t index,uint32_t &tcount) const
	{
		Transaction *ret = NULL;
		assert( index < mBlockCount );
		if ( index < mBlockCount )
		{
			ret = mBlocks[index];
			if ( (index+1) == mBlockCount )
			{
				tcount = (uint32_t)(&mTransactions[mTransactionCount] - ret);
			}
			else
			{
				tcount = (uint32_t)(mBlocks[index+1] - ret);
			}
		}
		return ret;
	}

	BitcoinAddress * getAddress(const uint8_t from[20],uint32_t &adr)
	{
		BitcoinAddress *ret = NULL;

		BitcoinAddress h(from);
		ret = mAddresses.find(h);

		if ( ret == NULL )
		{
			ret = mAddresses.insert(h);
		}
		if ( ret )
		{
			adr = mAddresses.getIndex(ret) + 1;
		}
		return ret;
	}

	uint32_t getAddressCount(void) const
	{
		return (uint32_t)mAddresses.size();
	}

	Transaction * getSingleTransaction(uint32_t index)
	{
		Transaction *ret = NULL;
		assert( index < MAX_TOTAL_TRANSACTIONS );
		assert( index < mTransactionCount );
		if ( index < mTransactionCount )
		{
			ret = &mTransactions[index];
		}
		return ret;
	}


	Transaction *getTransactions(uint32_t count)
	{
		init();
		Transaction *ret = NULL;
		assert( (mTransactionCount+count) < MAX_TOTAL_TRANSACTIONS );
		if ( (mTransactionCount+count) < MAX_TOTAL_TRANSACTIONS ) 
		{
			ret = &mTransactions[mTransactionCount];
			mTransactionCount+=count;
		}
		return ret;
	}

	TransactionInput * getInputs(uint32_t count)
	{
		TransactionInput *ret = NULL;
		assert( (mTotalInputCount+count) < MAX_TOTAL_INPUTS );
		if ( (mTotalInputCount+count) < MAX_TOTAL_INPUTS ) 
		{
			ret = &mInputs[mTotalInputCount];
			mTotalInputCount+=count;
		}
		return ret;
	}

	TransactionOutput *getOutputs(uint32_t count)
	{
		TransactionOutput *ret = NULL;
		assert( (mTotalOutputCount+count) < MAX_TOTAL_OUTPUTS );
		if ( (mTotalOutputCount+count) < MAX_TOTAL_OUTPUTS )
		{
			ret = &mOutputs[mTotalOutputCount];
			mTotalOutputCount+=count;
		}
		return ret;
	}

	TransactionOutput * getOutput(uint32_t index)
	{
		TransactionOutput *ret = NULL;
		assert( index < mTotalOutputCount );
		if ( index < mTotalOutputCount )
		{
			ret = &mOutputs[index];
		}
		return ret;
	}

	const char *getKey(uint32_t a) const
	{
		static char scratch[256];
		const char *ret = "UNKNOWN ADDRESS";
		if ( a )
		{
			BitcoinAddress *ba = mAddresses.getKey(a-1);
			uint8_t address1[25];
			BLOCKCHAIN_BITCOIN_ADDRESS::bitcoinRIPEMD160ToAddress((const uint8_t *)ba,address1);
			BLOCKCHAIN_BITCOIN_ADDRESS::bitcoinAddressToAscii(address1,scratch,256);
			ret = scratch;
		}
		return ret;
	}

	void printTransaction(uint32_t index,Transaction *t,uint32_t address)
	{

		uint64_t totalInput=0;
		uint64_t totalOutput=0;
		uint64_t coinBase=0;

		for (uint32_t i=0; i<t->mOutputCount; i++)
		{
			TransactionOutput &o = t->mOutputs[i];
			totalOutput+=o.mValue;
		}
		for (uint32_t i=0; i<t->mInputCount; i++)
		{
			TransactionInput &input = t->mInputs[i];
			if ( input.mOutput )
			{
				TransactionOutput &o = *input.mOutput;
				totalInput+=o.mValue;
			}
		}

		printf("    Transaction #%s From Block: %s has %s inputs and %s outputs time: %s.\r\n", formatNumber(index), formatNumber(t->mBlock), formatNumber(t->mInputCount), formatNumber(t->mOutputCount), getTimeString(t->mTime) );

		printf("    Total Input: %0.4f Total Output: %0.4f : Fees: %0.4f\r\n", 
			(float)totalInput / ONE_BTC, 
			(float) totalOutput / ONE_BTC, 
			(float)((totalOutput-totalInput)-coinBase) / ONE_BTC );

		for (uint32_t i=0; i<t->mInputCount; i++)
		{
			TransactionInput &input = t->mInputs[i];
			if ( input.mOutput )
			{
				TransactionOutput &o = *input.mOutput;
				if ( o.mAddress == address )
				{
					printf("        [Input] "); 
				}
				else
				{
					printf("         Input  "); 
				}
				printf("%d : %s[%d] : Value %0.4f\r\n", i, getKey(o.mAddress),o.mAddress, (float)o.mValue / ONE_BTC );
			}
			else
			{
				printf("        [Input] %d : COINBASE\r\n", i );
			}
		}

		for (uint32_t i=0; i<t->mOutputCount; i++)
		{
			TransactionOutput &o = t->mOutputs[i];
			if ( o.mAddress == address )
			{
				printf("        [Output] ");
			}
			else
			{
				printf("         Output  ");
			}
			printf("%d : %s[%d] : Value %0.4f\r\n", i, getKey(o.mAddress),o.mAddress, (float)o.mValue / ONE_BTC );
		}
	}

	void printTransactions(uint32_t blockIndex)
	{
		uint32_t tcount;
		Transaction *t = getBlock(blockIndex,tcount);
		if ( t )
		{
			printf("===================================================\r\n");
			printf("Block #%s has %s transactions.\r\n", formatNumber(blockIndex), formatNumber(tcount) );
			for (uint32_t j=0; j<tcount; j++)
			{
				printTransaction(j,t,0);
				t++;
			}
			printf("===================================================\r\n");
			printf("\r\n");
		}
	}

	void reportCounts(void)
	{
		if ( mTransactionCount )
		{
			printf("%s transactions.\r\n", formatNumber(mTransactionCount) );
			printf("%s inputs.\r\n", formatNumber(mTotalInputCount) );
			printf("%s outputs.\r\n", formatNumber(mTotalOutputCount) );
			printf("%s addresses.\r\n", formatNumber(mAddresses.size()) );

			enum StatType
			{
				ST_ZERO,
				ST_DUST,
				ST_LESSONE,
				ST_LESSTEN,
				ST_LESS_HUNDRED,
				ST_LESS_THOUSAND,
				ST_LESS_TEN_THOUSAND,
				ST_LESS_HUNDRED_THOUSAND,
				ST_GREATER_HUNDRED_THOUSAND,
				ST_LAST
			};
			uint32_t counts[ST_LAST];
			uint64_t balances[ST_LAST];
			for (uint32_t i=0; i<ST_LAST; i++)
			{
				counts[i] = 0;
				balances[i] = 0;
			}

			for (uint32_t i=0; i<mAddresses.size(); i++)
			{
				BitcoinAddress *ba = mAddresses.getKey(i);
				uint64_t balance = ba->mTotalReceived-ba->mTotalSent;
				uint32_t btc = (uint32_t)(balance/ONE_BTC);
				if ( balance == 0 )
				{
					counts[ST_ZERO]++;
				}
				else if ( balance < ONE_MBTC )
				{
					counts[ST_DUST]++;
					balances[ST_DUST]+=balance;
				}
				else if ( balance < ONE_BTC )
				{
					counts[ST_LESSONE]++;
					balances[ST_LESSONE]+=balance;
				}
				else if ( btc < 10 )
				{
					counts[ST_LESSTEN]++;
					balances[ST_LESSTEN]+=btc;
				}
				else if ( btc < 100 )
				{
					counts[ST_LESS_HUNDRED]++;
					balances[ST_LESS_HUNDRED]+=btc;
				}
				else if ( btc < 1000 )
				{
					counts[ST_LESS_THOUSAND]++;
					balances[ST_LESS_THOUSAND]+=btc;
				}
				else if ( btc < 10000 )
				{
					counts[ST_LESS_TEN_THOUSAND]++;
					balances[ST_LESS_TEN_THOUSAND]+=btc;
				}
				else if ( btc < 100000 )
				{
					counts[ST_LESS_HUNDRED_THOUSAND]++;
					balances[ST_LESS_HUNDRED_THOUSAND]+=btc;
				}
				else
				{
					counts[ST_GREATER_HUNDRED_THOUSAND]++;
					balances[ST_GREATER_HUNDRED_THOUSAND]+=btc;
				}
			}
			printf("Found %s addresses which have ever been used.\r\n", formatNumber(mAddresses.size()) );

			printf("Found %s addresses with a zero balance.\r\n", formatNumber( counts[ST_ZERO]) );

			printf("Found %s 'dust' addresses (less than 1mbtc) with a total balance of %0.5f BTC\r\n", 
				formatNumber( counts[ST_DUST] ), 
				(float)balances[ST_DUST] / ONE_BTC );

			printf("Found %s addresses with a balance greater than 1mbtc but less than 1btc, total balance %0.5f\r\n",
				formatNumber(counts[ST_LESSONE]),
				(float)balances[ST_LESSONE]/ONE_BTC );

			printf("Found %s addresses with a balance greater than 1btc but less than 10btc, total btc: %s\r\n",
				formatNumber(counts[ST_LESSTEN]),
				formatNumber((uint32_t)balances[ST_LESSTEN]));

			printf("Found %s addresses with a balance greater than 10btc but less than 100btc, total: %s\r\n",
				formatNumber(counts[ST_LESS_HUNDRED]),
				formatNumber((uint32_t)balances[ST_LESS_HUNDRED]));

			printf("Found %s addresses with a balance greater than 100btc but less than 1,000btc, total: %s\r\n",
				formatNumber(counts[ST_LESS_THOUSAND]),
				formatNumber((uint32_t)balances[ST_LESS_THOUSAND]));

			printf("Found %s addresses with a balance greater than 1,000btc but less than 10,000btc, total: %s\r\n",
				formatNumber(counts[ST_LESS_TEN_THOUSAND]),
				formatNumber((uint32_t)balances[ST_LESS_TEN_THOUSAND]));

			printf("Found %s addresses with a balance greater than 10,000btc but less than 100,000btc, total: %s\r\n",
				formatNumber(counts[ST_LESS_HUNDRED_THOUSAND]),
				formatNumber((uint32_t)balances[ST_LESS_HUNDRED_THOUSAND]));

			printf("Found %s addresses with a balance greater than 100,000btc, total: %s\r\n",
				formatNumber(counts[ST_GREATER_HUNDRED_THOUSAND]),
				formatNumber((uint32_t)balances[ST_GREATER_HUNDRED_THOUSAND]));


		}
	}


	void gatherTransaction(BitcoinAddress *ba,Transaction *t,uint32_t tindex)
	{
		if ( ba && ba->mTransactionIndex != tindex )
		{
			ba->mTransactionIndex = tindex;
			ba->mTransactions[ba->mTransactionCount] = t;
			ba->mTransactionCount++;
		}
	}

	void countTransaction(BitcoinAddress *ba,
						  uint32_t tindex,
						  uint32_t &transactionReferenceCount)
	{
		if ( ba && ba->mTransactionIndex != tindex )
		{
			ba->mTransactionIndex = tindex;
			ba->mTransactionCount++;
			transactionReferenceCount++;
		}
	}

	BitcoinAddress *getAddress(uint32_t a)
	{
		BitcoinAddress *ret = NULL;
		if ( a )
		{
			ret = mAddresses.getKey(a-1);
		}
		return ret;
	}


	void gatherAddresses(void)
	{
		delete []mTransactionReferences;
		mTransactionReferences = NULL;

		for (uint32_t i=0; i<mAddresses.size(); i++)
		{
			BitcoinAddress *ba = mAddresses.getKey(i);
			ba->mTransactionCount = 0;
			ba->mTransactionIndex = 0xFFFFFFFF;
			ba->mTransactions = NULL;
			ba->mLastInputTime = 0;
			ba->mLastOutputTime = 0;
			ba->mFirstOutputTime = 0;
			ba->mTotalReceived = 0;
			ba->mTotalSent = 0;
			ba->mInputCount = 0;
			ba->mOutputCount = 0;
		}

		uint32_t transactionReferenceCount=0;

		// ok..we.now it's time to add all transactions to all addresses...
		for (uint32_t i=0; i<mTransactionCount; i++)
		{
			Transaction &t = mTransactions[i];
			for (uint32_t j=0; j<t.mOutputCount; j++)
			{
				TransactionOutput &o = t.mOutputs[j];
				BitcoinAddress *ba = getAddress(o.mAddress);
				countTransaction(ba,i,transactionReferenceCount);
			}
			for (uint32_t j=0; j<t.mInputCount; j++)
			{
				TransactionInput &input = t.mInputs[j];
				if ( input.mOutput )
				{
					TransactionOutput &o = *input.mOutput;
					BitcoinAddress *ba = getAddress(o.mAddress);
					countTransaction(ba,i,transactionReferenceCount);
				}
			}
		}

		mTransactionReferences = new Transaction*[transactionReferenceCount];
		transactionReferenceCount=0;
		for (uint32_t i=0; i<mAddresses.size(); i++)
		{
			BitcoinAddress *ba = mAddresses.getKey(i);
			ba->mTransactions = &mTransactionReferences[transactionReferenceCount];
			transactionReferenceCount+=ba->mTransactionCount;
			ba->mTransactionIndex = 0xFFFFFFFF; // which transaction we have stored yet so far...
			ba->mTransactionCount = 0;
		}

		// ok..we.now it's time to add all transactions to all addresses...
		for (uint32_t i=0; i<mTransactionCount; i++)
		{
			Transaction &t = mTransactions[i];

			for (uint32_t j=0; j<t.mOutputCount; j++)
			{
				TransactionOutput &o = t.mOutputs[j];
				BitcoinAddress *ba = getAddress(o.mAddress);
				if ( ba )
				{
					gatherTransaction(ba,&t,i);
					ba->mTotalReceived+=o.mValue;
					ba->mOutputCount++;
					if ( t.mTime > ba->mLastOutputTime ) // if the transaction time is more recnet than the last output time..
					{
						ba->mLastOutputTime = t.mTime;
						if ( ba->mFirstOutputTime == 0 ) // if this is the first output encountered, then mark it as the first output time.
						{
							ba->mFirstOutputTime = t.mTime;
						}
					}

				}
			}

			for (uint32_t j=0; j<t.mInputCount; j++)
			{
				TransactionInput &input = t.mInputs[j];

				if ( input.mOutput )
				{
					TransactionOutput &o = *input.mOutput;
					BitcoinAddress *ba = getAddress(o.mAddress);
					if ( ba )
					{
						gatherTransaction(ba,&t,i);
						ba->mTotalSent+=o.mValue;
						ba->mInputCount++;
						if ( t.mTime > ba->mLastInputTime ) // if the transaction time is newer than the last input/spent time..
						{
							ba->mLastInputTime = t.mTime;
						}
					}
				}

			}
		}
	}

	void printTopBalances(uint32_t tcount,uint32_t minBalance)
	{
		uint32_t plotCount = 0;
		uint64_t mbtc = minBalance*ONE_BTC;
		for (uint32_t i=0; i<mAddresses.size(); i++) // print one in every 10,000 addresses (just for testing right now)
		{
			BitcoinAddress *ba = mAddresses.getKey(i);
			uint64_t balance = ba->getBalance();
			if ( balance >= mbtc )
			{
				plotCount++;
			}
		}

		if ( !plotCount )
		{
			printf("No addresses found with a balance more than %d bitcoins\r\n", minBalance );
			return;
		}

		BitcoinAddress **sortPointers = new BitcoinAddress*[plotCount];
		plotCount = 0;
		for (uint32_t i=0; i<mAddresses.size(); i++) // print one in every 10,000 addresses (just for testing right now)
		{
			BitcoinAddress *ba = mAddresses.getKey(i);
			uint64_t balance = ba->getBalance();
			if ( balance >= mbtc )
			{
				sortPointers[plotCount] = ba;
				plotCount++;
			}
		}
		SortByBalance sb(sortPointers,plotCount);
		time_t currentTime;
		time(&currentTime); // get the current time.

		if ( plotCount < tcount )
		{
			tcount = plotCount;
		}
		printf("Displaying the top %d addresses by balance.\r\n", tcount );
		printf("==============================================\r\n");
		printf(" Address           : Balance  : Days Since Last Use\r\n");
		printf("==============================================\r\n");
		for (uint32_t i=0; i<tcount; i++)
		{
			BitcoinAddress *ba = sortPointers[i];
			uint64_t balance = ba->mTotalReceived-ba->mTotalSent;
			uint32_t lastUsed = ba->mLastInputTime; // the last time we sent money (not received because anyone can send us money).
			if ( ba->mLastInputTime == 0 ) // if it has never had a spend.. then we use the time of first receive..
			{
				lastUsed = ba->mFirstOutputTime;
			}
			double seconds = difftime(currentTime,time_t(lastUsed));
			double minutes = seconds/60;
			double hours = minutes/60;
			uint32_t days = (uint32_t) (hours/24);
			uint32_t adr = mAddresses.getIndex(ba)+1;
			printf("%40s,  %8d,  %4d\r\n", getKey(adr), (uint32_t)( balance / ONE_BTC ), days );
		}
		delete []sortPointers;
	}

	void zombieReport(uint32_t zdays,uint32_t minBalance)
	{
		uint32_t zombie_count=0;
		uint32_t alive_count=0;
		uint32_t zombie_balance=0;
		uint32_t alive_balance=0;
		uint64_t mbtc = minBalance*ONE_BTC;
		time_t currentTime;
		time(&currentTime); // get the current time.
		printf("Scanning %s bitcoin addresses and looking for zombies.\r\n", formatNumber(mAddresses.size()));

		for (uint32_t i=0; i<mAddresses.size(); i++) // print one in every 10,000 addresses (just for testing right now)
		{
			BitcoinAddress *ba = mAddresses.getKey(i);
			uint64_t balance = ba->mTotalReceived-ba->mTotalSent;
			if ( balance > mbtc )
			{
				uint32_t btc = (uint32_t)((balance)/ONE_BTC);
				uint32_t lastUsed = ba->mLastInputTime; // the last time we sent money (not received because anyone can send us money).
				if ( ba->mLastInputTime == 0 ) // if it has never had a spend.. then we use the time of first receive..
				{
					lastUsed = ba->mFirstOutputTime;
				}
				double seconds = difftime(currentTime,time_t(lastUsed));
				double minutes = seconds/60;
				double hours = minutes/60;
				uint32_t days = (uint32_t) (hours/24);
				if ( days > zdays )
				{
					zombie_count++;
					zombie_balance+=btc;
				}
				else
				{
					alive_count++;
					alive_balance+=btc;
				}
			}
		}
		printf("Zombie Report:\r\n");
		printf("Found %s zombie addresses older than %s days with a total balance of %s, excluding balances less than %s\r\n",formatNumber(zombie_count),formatNumber(zdays),formatNumber(zombie_balance), formatNumber(minBalance) );
		printf("Found %s living addresses newer than %s days with a total balance of %s, excluding balances less than %s\r\n",formatNumber(alive_count),formatNumber(zdays),formatNumber(alive_balance), formatNumber(minBalance) );
	}

	void printOldest(uint32_t tcount,uint32_t minBalance)
	{
		uint32_t plotCount = 0;
		uint64_t mbtc = minBalance*ONE_BTC;
		for (uint32_t i=0; i<mAddresses.size(); i++) // print one in every 10,000 addresses (just for testing right now)
		{
			BitcoinAddress *ba = mAddresses.getKey(i);
			uint64_t balance = ba->mTotalReceived-ba->mTotalSent;
			if ( balance >= mbtc )
			{
				plotCount++;
			}
		}

		if ( !plotCount )
		{
			printf("No addresses found with a balance more than %d bitcoins\r\n", minBalance );
			return;
		}

		BitcoinAddress **sortPointers = new BitcoinAddress*[plotCount];
		plotCount = 0;
		for (uint32_t i=0; i<mAddresses.size(); i++) // print one in every 10,000 addresses (just for testing right now)
		{
			BitcoinAddress *ba = mAddresses.getKey(i);
			uint64_t balance = ba->mTotalReceived-ba->mTotalSent;
			if ( balance >= mbtc )
			{
				sortPointers[plotCount] = ba;
				plotCount++;
			}
		}
		SortByAge sb(sortPointers,plotCount);
		time_t currentTime;
		time(&currentTime); // get the current time.

		if ( plotCount < tcount )
		{
			tcount = plotCount;
		}
		printf("Displaying the top %d addresses by age.\r\n", tcount );
		printf("==============================================\r\n");
		printf(" Address           : Balance  : Days Since Last Use\r\n");
		printf("==============================================\r\n");
		for (uint32_t i=0; i<tcount; i++)
		{
			BitcoinAddress *ba = sortPointers[i];
			uint64_t balance = ba->mTotalReceived-ba->mTotalSent;
			uint32_t lastUsed = ba->mLastInputTime; // the last time we sent money (not received because anyone can send us money).
			if ( ba->mLastInputTime == 0 ) // if it has never had a spend.. then we use the time of first receive..
			{
				lastUsed = ba->mFirstOutputTime;
			}
			double seconds = difftime(currentTime,time_t(lastUsed));
			double minutes = seconds/60;
			double hours = minutes/60;
			uint32_t days = (uint32_t) (hours/24);
			uint32_t adr = mAddresses.getIndex(ba)+1;
			printf("%40s,  %8d,  %4d\r\n", getKey(adr), (uint32_t)( balance / ONE_BTC ), days );
		}
		delete []sortPointers;
	}


	void printAddress(const char *adr)
	{
		BitcoinAddress ba;
		uint8_t output[25];
		if ( BLOCKCHAIN_BITCOIN_ADDRESS::bitcoinAsciiToAddress(adr,output) )
		{
			memcpy(&ba.mWord0,&output[1],20);
			BitcoinAddress *found = mAddresses.find(ba);
			if ( found )
			{
				printAddress(found);
			}
			else
			{
				printf("Failed to locate address: %s\r\n", adr );
			}
		}
		else
		{
			printf("Failed to decode address: %s\r\n", adr );
		}
	}

	void printAddresses(void)
	{
		for (uint32_t i=0; i<mAddresses.size(); i++) // print one in every 10,000 addresses (just for testing right now)
		{
			BitcoinAddress *ba = mAddresses.getKey(i);
			printAddress(ba);
		}
	}

	void printAddress(BitcoinAddress *ba)
	{
		uint32_t i = mAddresses.getIndex(ba);
		printf("========================================\r\n");
		printf("PublicKey: %s[%d] has %s transactions associated with it.\r\n", getKey(i+1),i+1, formatNumber(ba->mTransactionCount) );
		printf("Balance: %0.4f : TotalReceived: %0.4f TotalSpent: %0.4f\r\n", (float) (ba->mTotalReceived-ba->mTotalSent)/ONE_BTC, (float)ba->mTotalReceived / ONE_BTC, (float) ba->mTotalSent / ONE_BTC );
		if ( ba->mLastInputTime )
		{
			printf("Last Input Time: %s\r\n", getTimeString(ba->mLastInputTime) );
		}
		if ( ba->mLastOutputTime )
		{
			printf("Last Output Time: %s\r\n", getTimeString(ba->mLastOutputTime) );
		}
		for (uint32_t j=0; j<ba->mTransactionCount; j++)
		{
			printTransaction(j,ba->mTransactions[j],i+1);
		}
		printf("========================================\r\n");
		printf("\r\n");
	}

	inline StatSize getStatSize(uint64_t v)
	{
		if ( v == 0 ) return SS_ZERO;
		for (uint32_t i=1; i<(SS_COUNT-1); i++)
		{
			if ( v < mStatLimits[i] )
			{
				return (StatSize)i;
			}
		}
		return SS_MAX_BTC;
	}

	void gatherStatistics(uint32_t stime,uint32_t zombieDate,bool recordAddresses)
	{
		gatherAddresses();

		assert( mStatCount < MAX_STAT_COUNT );
		StatRow &row = mStatistics[mStatCount];
		row.mTime = stime;

		time_t currentTime;
		time(&currentTime); // get the current time.

		for (uint32_t i=0; i<mAddresses.size(); i++) // print one in every 10,000 addresses (just for testing right now)
		{
			BitcoinAddress *ba = mAddresses.getKey(i);
			uint64_t balance = ba->mTotalReceived-ba->mTotalSent;
			StatSize s = getStatSize(balance);
			row.mCount++;
			row.mValue+=balance;

			row.mStats[s].mCount++;
			row.mStats[s].mValue+=balance;

			uint32_t lastUsed = ba->mLastInputTime; // the last time we sent money (not received because anyone can send us money).
			if ( ba->mLastInputTime == 0 ) // if it has never had a spend.. then we use the time of first receive..
			{
				lastUsed = ba->mFirstOutputTime;
			}

			if ( lastUsed < zombieDate )
			{
				row.mZombieTotal+=balance;
				row.mZombieCount++;
			}

		}

		if ( recordAddresses )
		{
			// First compute how many bitcoin addresses have a balance over a minimum bitcoin size (we will start with just one)
			uint32_t plotCount = 0;

			for (uint32_t i=0; i<mAddresses.size(); i++) 
			{
				BitcoinAddress *ba = mAddresses.getKey(i);
				uint64_t balance = ba->mTotalReceived-ba->mTotalSent;
				if ( balance >= ONE_BTC )
				{
					plotCount++;
				}
			}

			printf("Gathering Address Delta's for %s addresses containing more than one bitcoin\r\n", formatNumber(plotCount) );

			if ( plotCount )
			{
				BitcoinAddress **sortPointers = new BitcoinAddress*[plotCount];
				uint32_t plotCount = 0;
				for (uint32_t i=0; i<mAddresses.size(); i++) // print one in every 10,000 addresses (just for testing right now)
				{
					BitcoinAddress *ba = mAddresses.getKey(i);
					uint64_t balance = ba->mTotalReceived-ba->mTotalSent;
					if ( balance >= ONE_BTC )
					{
						sortPointers[plotCount] = ba;
						plotCount++;
					}
				}

				uint32_t reportCount = plotCount;

				row.mAddresses = new StatAddress[reportCount];
				row.mAddressCount = reportCount;

				SortByBalance sb(sortPointers,plotCount);

				for (uint32_t i=0; i<reportCount; i++)
				{
					BitcoinAddress *ba = sortPointers[i];
					StatAddress &sa = row.mAddresses[i];
					sa.mAddress = mAddresses.getIndex(ba)+1;
					sa.mLastTime = ba->getLastUsedTime();
					sa.mFirstTime = ba->mFirstOutputTime;
					sa.mTotalReceived = (uint32_t)(ba->mTotalReceived/ONE_MBTC);
					sa.mTotalSent = (uint32_t)(ba->mTotalSent/ONE_MBTC);
					sa.mTransactionCount = (uint8_t) (ba->mTransactionCount > 255 ? 255 : ba->mTransactionCount);
					uint32_t inputCount = ba->getInputCount();
					uint32_t outputCount = ba->getOutputCount();
					sa.mInputCount = (uint8_t) (inputCount > 255 ? 255 : inputCount);
					sa.mOutputCount = (uint8_t) (outputCount > 255 ? 255 : outputCount);
				}

				if ( mStatCount >= 1 )
				{
					// build the delta!
					StatRow &previousRow = mStatistics[mStatCount-1];
					//
					uint32_t *addressIndex = new uint32_t[mAddresses.size()];	// convert from master address list to used by statistics address list table

					memset(addressIndex,0,sizeof(uint32_t)*mAddresses.size());

					for (uint32_t i=0; i<previousRow.mAddressCount; i++)
					{
						StatAddress &sa = previousRow.mAddresses[i];
						addressIndex[sa.mAddress-1] = i+1; // make this address as being used by the previous row
					}

					uint32_t changeCount=0;
					uint32_t newCount=0;
					uint32_t deleteCount=0;
					uint32_t sameCount=0;
					uint32_t riseFromTheDeadCount=0;
					uint32_t riseFromTheDeadAmount=0;

					for (uint32_t i=0; i<row.mAddressCount; i++)
					{
						StatAddress &na = row.mAddresses[i];	// get the current row address
						uint32_t aindex = na.mAddress-1; // get the array index for this address

						if ( addressIndex[aindex] ) // did the previous row use this address?
						{
							uint32_t pindex = addressIndex[aindex]-1; // get the array index in the previous row so we can do a compare.

							addressIndex[aindex] = 0xFFFFFFFF; // make it as processed.

							StatAddress &oa = previousRow.mAddresses[pindex];

							if ( na == oa )
							{
								sameCount++; // no changes...
							}
							else
							{
								// ok.. if it previously existed but there has been a change, was it a zombie change?
								if ( oa.mLastTime < zombieDate && na.mLastTime >= zombieDate )
								{
									riseFromTheDeadCount++;
									riseFromTheDeadAmount+=oa.getBalance(); // how much bitcoin rose from the dead.
								}
								changeCount++;
							}
						}
						else
						{
							newCount++;
						}
					}

					for (uint32_t i=0; i<mAddresses.size(); i++)
					{
						if ( addressIndex[i] && addressIndex[i] != 0xFFFFFFFF )
						{
							deleteCount++;
						}
					}

					row.mNewAddressCount = newCount;
					row.mDeleteAddressCount = deleteCount;
					row.mChangeAddressCount = changeCount;
					row.mSameAddressCount = sameCount;
					row.mRiseFromDeadAmount = riseFromTheDeadAmount;
					row.mRiseFromDeadCount = riseFromTheDeadCount;


					if ( newCount )
					{
						row.mNewAddresses = new StatAddress[newCount];
					}

					if ( changeCount )
					{
						row.mChangedAddresses = new StatAddress[changeCount];
					}
					// Assign an array index to each address used by the previous row
					memset(addressIndex,0,sizeof(uint32_t)*mAddresses.size());
					for (uint32_t i=0; i<previousRow.mAddressCount; i++)
					{
						StatAddress &sa = previousRow.mAddresses[i];
						addressIndex[sa.mAddress-1] = i+1;
					}

					newCount = 0;
					changeCount = 0;
					sameCount = 0;

					for (uint32_t i=0; i<row.mAddressCount; i++)
					{
						StatAddress &na = row.mAddresses[i];
						uint32_t aindex = na.mAddress-1;

						if ( addressIndex[aindex] ) // did the previous row have this address.
						{
							uint32_t pindex = addressIndex[aindex]-1;
							addressIndex[aindex] = 0xFFFFFFFF; // make it as processed.
							StatAddress &oa = previousRow.mAddresses[pindex];
							if ( na == oa )
							{
								sameCount++;
							}
							else
							{
								row.mChangedAddresses[changeCount] = na;
								changeCount++;
							}
						}
						else
						{
							row.mNewAddresses[newCount] = na;
							newCount++;
						}
					}

					if ( deleteCount ) 
					{
						row.mDeletedAddresses = new uint32_t[deleteCount];
						deleteCount = 0;
						for (uint32_t i=0; i<mAddresses.size(); i++)
						{
							if ( addressIndex[i] && addressIndex[i] != 0xFFFFFFFF )
							{
								row.mDeletedAddresses[deleteCount] = i+1;
								deleteCount++;
							}
						}
					}

					delete []addressIndex;

					printf("Found %s new addresses, %s changed addresses, and %s deleted addresses\r\n",
						formatNumber(newCount),
						formatNumber(changeCount),
						formatNumber(deleteCount));

					if ( mStatCount >= 2 )
					{
						StatRow &oldRow = mStatistics[mStatCount-2];
						delete []oldRow.mAddresses;
						oldRow.mAddresses = NULL;
						oldRow.mAddressCount = 0;
					}
				}
				delete []sortPointers;
			}
		}
		mStatCount++;
	}

	void saveAddressesOverTime(void)
	{
		FILE *fph = fopen("BlockChainAddresses.bin", "wb");
		if ( fph )
		{
			const char *header = "BLOCK_CHAIN_ADDRESSES";
			fwrite(header,strlen(header)+1,1,fph);
			uint32_t version = 1;
			fwrite(&version, sizeof(version), 1, fph );

			// first..we need to count how many addresses are being used for this report.
			uint32_t *addressIndex = new uint32_t[mAddresses.size()];	// convert from master address list to used by statistics address list table
			uint32_t *inverseAddress = new uint32_t[mAddresses.size()]; // convert from remapped list back to master address.

			memset(addressIndex,0,sizeof(uint32_t)*mAddresses.size());
			memset(inverseAddress,0,sizeof(uint32_t)*mAddresses.size());

			uint32_t fcount = 0; // number of unique addresses found

			for (uint32_t i=0; i<mStatCount; i++)
			{
				StatRow &row = mStatistics[i];
				for (uint32_t i=0; i<row.mAddressCount; i++)
				{
					StatAddress &sa = row.mAddresses[i];
					assert( sa.mAddress	 );
					uint32_t a = sa.mAddress-1;
					assert( a < mAddresses.size() );
					if ( addressIndex[a] == 0 ) // if this is the first time we have encountered this address, then we add it to the forward and inverse lookup tables.
					{
						addressIndex[a] = fcount+1;
						inverseAddress[fcount] = a;
						fcount++;
					}
				}
			}
			// ok..now ready to write out the address headers.
			fwrite(&fcount,sizeof(fcount),1,fph);	// save how many unique addresses were found.
			// now write out the 20 byte public key address for each.
			for (uint32_t i=0; i<fcount; i++)
			{
				uint32_t a = inverseAddress[i]; // get the original address.
				BitcoinAddress *ba = mAddresses.getKey(a);
				fwrite(ba,20,1,fph);// write out the bitcoin address 160 bit public key (20 bytes long)
			}

			fwrite(&mStatCount, sizeof(mStatCount), 1, fph );

			for (uint32_t i=0; i<mStatCount; i++)
			{
				StatRow &row = mStatistics[i];
				fwrite(&row.mTime,sizeof(row.mTime),1, fph); // write out the time that this row was recorded...
				if ( i == 0 ) 
				{
					fwrite(&row.mAddressCount,sizeof(row.mAddressCount),1,fph);
				}
				else
				{
					fwrite(&row.mNewAddressCount,sizeof(row.mNewAddressCount),1,fph);
				}
				fwrite(&row.mChangeAddressCount,sizeof(row.mChangeAddressCount),1,fph);
				fwrite(&row.mDeleteAddressCount,sizeof(row.mDeleteAddressCount),1,fph);
			}

			for (uint32_t i=0; i<mStatCount; i++)
			{
				StatRow &row = mStatistics[i];
				if ( i == 0 )
				{
					for (uint32_t i=0; i<row.mAddressCount; i++)
					{
						StatAddress sa = row.mAddresses[i];
						sa.mAddress = addressIndex[sa.mAddress-1]; // convert to the new sequentially ordered hash
						fwrite(&sa,sizeof(sa),1,fph); // write it out...
					}
				}
				else
				{
					for (uint32_t i=0; i<row.mNewAddressCount; i++)
					{
						StatAddress sa = row.mNewAddresses[i];
						sa.mAddress = addressIndex[sa.mAddress-1]; // convert to the new sequentially ordered hash
						fwrite(&sa,sizeof(sa),1,fph); // write it out...
					}
					for (uint32_t i=0; i<row.mChangeAddressCount; i++)
					{
						StatAddress sa = row.mChangedAddresses[i];
						sa.mAddress = addressIndex[sa.mAddress-1]; // convert to the new sequentially ordered hash
						fwrite(&sa,sizeof(sa),1,fph); // write it out...
					}
					for (uint32_t i=0; i<row.mDeleteAddressCount; i++)
					{
						uint32_t da = row.mDeletedAddresses[i];
						uint32_t a = addressIndex[da-1];
						fwrite(&a,sizeof(uint32_t),1,fph);
					}
				}
			}
			delete []addressIndex;
			delete []inverseAddress;
			fclose(fph);
		}
	}

	void saveStatistics(bool record_addresses)
	{
		if ( record_addresses )
		{
			saveAddressesOverTime();
		}
		FILE *fph = fopen("stats.csv", "wb");
		if ( !fph ) 
		{
			printf("Failed to open statistics output file 'stats.csv' for write access.\r\n");
			return;
		}

// BitcoinAddress Summary Results first

		AgeStat ageStats[AM_LAST];

		// First compute how many bitcoin addresses have a balance over a minimum bitcoin size (we will start with just one)
		uint32_t plotCount = 0;
		for (uint32_t i=0; i<mAddresses.size(); i++) // print one in every 10,000 addresses (just for testing right now)
		{
			BitcoinAddress *ba = mAddresses.getKey(i);
			uint64_t balance = ba->mTotalReceived-ba->mTotalSent;

			uint32_t days = ba->getDaysSinceLastUsed();
			if ( days <= 1 )
			{
				ageStats[AM_ONE_DAY].addValue(balance);
			}
			else if ( days <= 7 )
			{
				ageStats[AM_ONE_WEEK].addValue(balance);
			}
			else if ( days <= 30 )
			{
				ageStats[AM_ONE_MONTH].addValue(balance);
			}
			else if ( days <= (30*3) )
			{
				ageStats[AM_THREE_MONTHS].addValue(balance);
			}
			else if ( days <= (30*6) )
			{
				ageStats[AM_SIX_MONTHS].addValue(balance);
			}
			else if ( days <= 365 )
			{
				ageStats[AM_ONE_YEAR].addValue(balance);
			}
			else if ( days <= 365*2 )
			{
				ageStats[AM_TWO_YEARS].addValue(balance);
			}
			else if ( days <= 365*3 )
			{
				ageStats[AM_THREE_YEARS].addValue(balance);
			}
			else if ( days <= 365*4 )
			{
				ageStats[AM_FOUR_YEARS].addValue(balance);
			}
			else
			{
				ageStats[AM_FIVE_YEARS].addValue(balance);
			}
			if ( balance >= ONE_BTC )
			{
				plotCount++;
			}
		}



		if ( plotCount )
		{
#define MAX_PLOT_COUNT 150000
			BitcoinAddress **sortPointers = new BitcoinAddress*[plotCount];
			uint32_t plotCount = 0;
			for (uint32_t i=0; i<mAddresses.size(); i++) // print one in every 10,000 addresses (just for testing right now)
			{
				BitcoinAddress *ba = mAddresses.getKey(i);
				uint64_t balance = ba->mTotalReceived-ba->mTotalSent;
				if ( balance >= ONE_BTC )
				{
					sortPointers[plotCount] = ba;
					plotCount++;
				}
			}
			uint32_t reportCount = plotCount;
			fprintf(fph,"\r\n");
			fprintf(fph,"\"BlockChain Statistics Report generated by: https://code.google.com/p/blockchain/ \"\r\n" );
			fprintf(fph,"\r\n");
			fprintf(fph,"\"Written by John W. Ratcliff mailto:jratcliffscarab@gmail.com\"\r\n" );
			fprintf(fph,"\"Website: http://codesuppository.blogspot.com/\"\r\n" );
			fprintf(fph,"\"TipJar Address: 1BT66EoaGySkbY9J6MugvQRhMMXDwPxPya\"\r\n" );

			fprintf(fph,"\r\n");
			fprintf(fph,"Bitcoin value distribution based on age.\r\n");
			fprintf(fph,"Age,Value,Count\r\n");
			for (uint32_t i=0; i<AM_LAST; i++)
			{
				AgeStat &as = ageStats[i];
				fprintf(fph,"%s,%d,%d\r\n", getAgeString((AgeMarker)i), (uint32_t)(as.mTotalValue/ONE_BTC), as.mCount );
			}
			fprintf(fph,"\r\n");


			fprintf(fph,"\"Found %s addreses with a bitcoin balance of 1btc or more.\"\r\n", formatNumber(plotCount));
			if ( plotCount > MAX_PLOT_COUNT )
			{
				reportCount = MAX_PLOT_COUNT;
				fprintf(fph,"\"Exceeded maximum plot count, so only reporting the first %s addresses.\"\r\n", formatNumber(reportCount));
			}
			{
				fprintf(fph,"\"Scatter Plot Data values of %s bitcoin address balances with over 1btc and number of days since last transaction. Sorted by Balance\"\r\n", formatNumber(plotCount));
				fprintf(fph,"Days,Value,FirstUsed,LastReceived,LastSpent,TotalSent,TotalReceived,TransactionCount,PublicKeyAddress\r\n");
				SortByBalance sb(sortPointers,plotCount);
				time_t currentTime;
				time(&currentTime); // get the current time.
				for (uint32_t i=0; i<reportCount; i++)
				{
					BitcoinAddress *ba = sortPointers[i];
					uint64_t balance = ba->mTotalReceived-ba->mTotalSent;
					uint32_t lastUsed = ba->mLastInputTime; // the last time we sent money (not received because anyone can send us money).
					if ( ba->mLastInputTime == 0 ) // if it has never had a spend.. then we use the time of first receive..
					{
						lastUsed = ba->mFirstOutputTime;
					}
					double seconds = difftime(currentTime,time_t(lastUsed));
					double minutes = seconds/60;
					double hours = minutes/60;
					uint32_t days = (uint32_t) (hours/24);
					uint32_t adr = mAddresses.getIndex(ba)+1;

					fprintf(fph,"%d,", days );
					fprintf(fph,"%d,", (uint32_t)( balance / ONE_BTC ));
					fprintf(fph,"\"%s\",", getTimeString(ba->mFirstOutputTime));
					fprintf(fph,"\"%s\",", getTimeString(ba->mLastOutputTime));
					fprintf(fph,"\"%s\",", getTimeString(ba->mLastInputTime));
					fprintf(fph,"%0.2f,", (float)ba->mTotalSent / ONE_BTC );
					fprintf(fph,"%0.2f,", (float) ba->mTotalReceived / ONE_BTC );
					fprintf(fph,"%d,", ba->mTransactionCount );
					fprintf(fph,"%s\r\n", getKey(adr) );

				}
				fprintf(fph,"\r\n");
			}
			{
				fprintf(fph,"\"Scatter Plot Data values of %s bitcoin address balances with over 1btc and number of days since last transaction. Sorted by Age\"\r\n", formatNumber(plotCount));
				fprintf(fph,"Days,Value,FirstUsed,LastReceived,LastSpent,TotalSent,TotalReceived,TransactionCount,PublicKeyAddress\r\n");

				SortByAge sb(sortPointers,plotCount);
				time_t currentTime;
				time(&currentTime); // get the current time.
				for (uint32_t i=0; i<reportCount; i++)
				{
					BitcoinAddress *ba = sortPointers[i];
					uint64_t balance = ba->mTotalReceived-ba->mTotalSent;
					uint32_t days = ba->getDaysSinceLastUsed();
					uint32_t adr = mAddresses.getIndex(ba)+1;
					fprintf(fph,"%d,", days );
					fprintf(fph,"%d,", (uint32_t)( balance / ONE_BTC ));
					fprintf(fph,"\"%s\",", getTimeString(ba->mFirstOutputTime));
					fprintf(fph,"\"%s\",", getTimeString(ba->mLastOutputTime));
					fprintf(fph,"\"%s\",", getTimeString(ba->mLastInputTime));
					fprintf(fph,"%0.2f,", (float)ba->mTotalSent / ONE_BTC );
					fprintf(fph,"%0.2f,", (float) ba->mTotalReceived / ONE_BTC );
					fprintf(fph,"%d,", ba->mTransactionCount );
					fprintf(fph,"%s\r\n", getKey(adr) );

				}
				fprintf(fph,"\r\n");
			}
			delete []sortPointers;
		}
//


		const char *months[12] = { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };
		{
			fprintf(fph,"\r\n");
			fprintf(fph,"Summary statistics for %d months.\r\n", mStatCount );
			fprintf(fph,"\r\n");


			fprintf(fph,"Date,");					// Date of this row
			fprintf(fph,"ZombieValue,");			// The total value of all zombie addresses
			fprintf(fph,"ZombieCount,");			// The number of zombie addresses
			fprintf(fph,"TotalBTC,");				// Total BTC in circulation
			fprintf(fph,"TotalActiveBTC,");			// Total non-zombie BTC
			fprintf(fph,"AddressesUsed,");			// Total addresses used.
			fprintf(fph,"AddressesWithBalance,");	// Total non-zero addresses.
			fprintf(fph,"ZeroBalanceCount,");		// Number of zero balance addresses.
			fprintf(fph,"DustCount,");				// The number of dust addresses < 1mbtc
			fprintf(fph,"DustAmount,");				// The total amount in all dust addresses.
			fprintf(fph,"AddressOneBTCCount,");		// The number of addresses with at least 1btc or more.
			fprintf(fph,"AddressOneBTCValue,");		// The total value of all addresses with at least 1btc or more.
			fprintf(fph,"Address1KCount,");			// The number of addresses with more than 1,000 btc in them.
			fprintf(fph,"Address1KValue,");		// The total value of all addresses with more than 1,000 btc
			fprintf(fph,"NewAddressCount,");		// Number of new addresses with a non-zero balance created
			fprintf(fph,"ZeroAddressCount,");		// number of addresses who's balance went down to zero.
			fprintf(fph,"ChangeAddressCount,");		// number of addresses who had changes against them.
			fprintf(fph,"SameAddressCount,");		// The number of addresses with no change.
			fprintf(fph,"RiseFromDeadCount,");		// number of addresses which rose from the dead.
			fprintf(fph,"RiseFromDeadAmount\r\n");	// 

			for (uint32_t i=0; i<mStatCount; i++)
			{
				StatRow &row = mStatistics[i];
				time_t tnow(row.mTime);
				struct tm beg;
				beg = *localtime(&tnow);

				uint32_t addressUsed = row.mCount;
				uint32_t zeroCount = row.mStats[SS_ZERO].mCount;
				uint32_t addressWithBalance = addressUsed - zeroCount;

				uint32_t dustCount = row.mStats[SS_ONE_MBTC].mCount;
				uint64_t dustAmount = row.mStats[SS_ONE_MBTC].mValue;

				uint32_t addressOneBTCCount = 0;
				uint64_t addressOneBTCTotal = 0;
				uint32_t addressOneKCount = 0;
				uint64_t addressOneKTotal = 0;

				for (uint32_t i=SS_FIVE_BTC; i<SS_MAX_BTC; i++)
				{
					addressOneBTCCount+=row.mStats[i].mCount;
					addressOneBTCTotal+=row.mStats[i].mValue;
					if ( i > SS_ONE_THOUSAND_BTC )
					{
						addressOneKCount+=row.mStats[i].mCount;
						addressOneKTotal+=row.mStats[i].mValue;
					}
				}

				
				// Date
				fprintf(fph,"\"%s %d %d\",",months[beg.tm_mon], beg.tm_mday, beg.tm_year+1900); 
				fprintf(fph,"%d,", (uint32_t)(row.mZombieTotal/ONE_BTC) );
				fprintf(fph,"%d,", row.mZombieCount );

				fprintf(fph,"%d,", (uint32_t)(row.mValue / ONE_BTC) );
				fprintf(fph,"%d,", (uint32_t)((row.mValue-row.mZombieTotal)/ ONE_BTC) );

				fprintf(fph,"%d,", addressUsed );
				fprintf(fph,"%d,", addressWithBalance );
				fprintf(fph,"%d,", zeroCount );
				fprintf(fph,"%d,", dustCount );
				fprintf(fph,"%d,", (uint32_t)(dustAmount / ONE_BTC) );
				fprintf(fph,"%d,", addressOneBTCCount );
				fprintf(fph,"%d,", (uint32_t) (addressOneBTCTotal / ONE_BTC) );
				fprintf(fph,"%d,", addressOneKCount );
				fprintf(fph,"%d,", (uint32_t) (addressOneKTotal / ONE_BTC) );

				fprintf(fph,"%d,", row.mNewAddressCount );
				fprintf(fph,"%d,", row.mDeleteAddressCount );
				fprintf(fph,"%d,", row.mChangeAddressCount );
				fprintf(fph,"%d,", row.mSameAddressCount );
				fprintf(fph,"%d,", row.mRiseFromDeadCount );
				fprintf(fph,"%d,", row.mRiseFromDeadAmount );

				fprintf(fph,"\r\n");
			}

			fprintf(fph,"\r\n");
		}
	
		{
			fprintf(fph,"\r\n");
			fprintf(fph,"Bitcoin Public Key Address Distribution Count by balance.\r\n");
			fprintf(fph,"\r\n");
			fprintf(fph,",");
			for (uint32_t i=0; i<SS_COUNT; i++)
			{
				fprintf(fph,"\"%s\",", mStatLabel[i]);
			}
			fprintf(fph,"\r\n");
			for (uint32_t i=0; i<mStatCount; i++)
			{
				StatRow &row = mStatistics[i];
				time_t tnow(row.mTime);
				struct tm beg;
				beg = *localtime(&tnow);
				fprintf(fph,"\"%s %d %d\",", months[beg.tm_mon], beg.tm_mday, beg.tm_year+1900);
				for (uint32_t j=0; j<SS_COUNT; j++)
				{
					StatValue &v = row.mStats[j];
					fprintf(fph,"%d,",v.mCount);
				}
				fprintf(fph,"\r\n");
			}
			fprintf(fph,"\r\n");
		}

		{
			fprintf(fph,"\r\n");
			fprintf(fph,"Bitcoin Public Key Address Distribution Total Value by balance.\r\n");
			fprintf(fph,"\r\n");
			fprintf(fph,",");
			for (uint32_t i=0; i<SS_COUNT; i++)
			{
				fprintf(fph,"\"%s\",", mStatLabel[i]);
			}
			fprintf(fph,"\r\n");
			for (uint32_t i=0; i<mStatCount; i++)
			{
				StatRow &row = mStatistics[i];
				time_t tnow(row.mTime);
				struct tm beg;
				beg = *localtime(&tnow);
				fprintf(fph,"\"%s %d %d\",", months[beg.tm_mon], beg.tm_mday, beg.tm_year+1900);
				for (uint32_t j=0; j<SS_COUNT; j++)
				{
					StatValue &v = row.mStats[j];
					fprintf(fph,"%0.4f,",(float)v.mValue/ONE_BTC);
				}
				fprintf(fph,"\r\n");
			}
			fprintf(fph,"\r\n");
		}


		{
			fprintf(fph,"\r\n");
			fprintf(fph,"NORMALIZED : Bitcoin Public Key Address Distribution Count by balance.\r\n");
			fprintf(fph,"\r\n");
			fprintf(fph,",");
			for (uint32_t i=1; i<SS_COUNT; i++)
			{
				fprintf(fph,"\"%s\",", mStatLabel[i]);
			}
			fprintf(fph,"\r\n");
			for (uint32_t i=0; i<mStatCount; i++)
			{
				StatRow &row = mStatistics[i];
				time_t tnow(row.mTime);
				struct tm beg;
				beg = *localtime(&tnow);
				fprintf(fph,"\"%s %d\",", months[beg.tm_mon], beg.tm_year+1900);
				for (uint32_t j=1; j<SS_COUNT; j++)
				{
					StatValue &v = row.mStats[j];
					float value = (float)v.mCount*100.0f / (float)(row.mCount-row.mStats[0].mCount);
					fprintf(fph,"%0.4f,", value );
				}
				fprintf(fph,"\r\n");
			}
			fprintf(fph,"\r\n");
		}

		{
			fprintf(fph,"\r\n");
			fprintf(fph,"NORMALIZED: Bitcoin Public Key Address Distribution Total Value by balance.\r\n");
			fprintf(fph,"\r\n");
			fprintf(fph,",");
			for (uint32_t i=1; i<SS_COUNT; i++)
			{
				fprintf(fph,"\"%s\",", mStatLabel[i]);
			}
			fprintf(fph,"\r\n");
			for (uint32_t i=0; i<mStatCount; i++)
			{
				StatRow &row = mStatistics[i];
				time_t tnow(row.mTime);
				struct tm beg;
				beg = *localtime(&tnow);
				fprintf(fph,"\"%s %d\",", months[beg.tm_mon], beg.tm_year+1900);
				for (uint32_t j=1; j<SS_COUNT; j++)
				{
					StatValue &v = row.mStats[j];
					float value = (float)(v.mValue)*100.0f / (float)row.mValue;
					fprintf(fph,"%0.4f,",value);
				}
				fprintf(fph,"\r\n");
			}
			fprintf(fph,"\r\n");
		}


		fclose(fph);
	}

protected:
	BitcoinAddressHashMap		mAddresses;				// A hash map of every single bitcoin address ever referenced to a much shorter integer to save memory

	uint32_t					mTransactionCount;
	uint32_t					mTotalInputCount;
	uint32_t					mTotalOutputCount;
	Transaction					*mTransactions;
	TransactionInput			*mInputs;
	TransactionOutput			*mOutputs;
	uint32_t					mBlockCount;
	Transaction					**mBlocks;
	Transaction					**mTransactionReferences;
	uint32_t					mStatCount;
	StatRow						mStatistics[MAX_STAT_COUNT];
	const char					*mStatLabel[SS_COUNT];
	uint64_t					mStatLimits[SS_COUNT];
};

#pragma warning(pop)

// This is the implementation of the BlockChain parser interface
class BlockChainImpl : public BlockChain
{
public:
	BlockChainImpl(const char *rootPath)
	{
		sprintf(mRootDir,"%s",rootPath);
		mCurrentBlockData = mBlockDataBuffer;	// scratch buffers to read in up to 3 block.
		mTransactionCount = 0;
		mBlockIndex = 0;
		mBlockBase = 0;
		mReadCount = 0;
		for (uint32_t i=0; i<MAX_BLOCK_FILES; i++)
		{
			mBlockChain[i] = NULL;
		}
		mBlockCount = 0;
		mScanCount = 0;
		mBlockHeaders = NULL;
		mLastBlockHeaderCount = 0;
		mLastBlockHeader = NULL;
		mTotalInputCount = 0;
		mTotalOutputCount = 0;
		mTotalTransactionCount = 0;
		openBlock();	// open the input file
	}

	// Close all blockchain files which have been opended so far
	virtual ~BlockChainImpl(void)
	{
		for (uint32_t i=0; i<mBlockIndex; i++)
		{
			if ( mBlockChain[i] )
			{
				fclose(mBlockChain[i]);	// close the block-chain file pointer
			}
		}
		delete []mBlockHeaders;
	}

	// Open the next data file in the block-chain sequence
	bool openBlock(void)
	{
		bool ret = false;

		char scratch[512];
#ifdef _MSC_VER
		sprintf(scratch,"%s\\blk%05d.dat", mRootDir, mBlockIndex );	// get the filename
#else
		sprintf(scratch,"%s/blk%05d.dat", mRootDir, mBlockIndex );	// get the filename
#endif
		FILE *fph = fopen(scratch,"rb");
		if ( fph )
		{
			fseek(fph,0L,SEEK_END);
			mFileLength = ftell(fph);
			fseek(fph,0L,SEEK_SET);
			mBlockChain[mBlockIndex] = fph;
			ret = true;
			printf("Successfully opened block-chain input file '%s'\r\n", scratch );
		}
		else
		{
			printf("Failed to open block-chain input file '%s'\r\n", scratch );
		}
		if ( mBlockHeaderMap.size() )
		{
			printf("Scanned %s block headers so far, %s since last time.\r\n", formatNumber(mBlockHeaderMap.size()), formatNumber(mBlockHeaderMap.size()-mLastBlockHeaderCount));
		}

		mLastBlockHeaderCount = mBlockHeaderMap.size();

		return ret;
	}

	virtual void release(void) 
	{
		delete this;
	}

	// Returns true if we successfully opened the block-chain input file
	bool isValid(void)
	{
		return mBlockChain[0] ? true : false;
	}

	void processTransactions(Block &block)
	{
		mTotalTransactionCount+=block.transactionCount;
		for (uint32_t i=0; i<block.transactionCount; i++)
		{
			BlockTransaction &t = block.transactions[i];
			Hash256 hash(t.transactionHash);
			FileLocation f(hash,t.fileIndex,t.fileOffset,t.transactionLength,t.transactionIndex);
			mTransactionMap.insert(f);
		}
		// ok.. now make sure we can locate every input transaction!
		for (uint32_t i=0; i<block.transactionCount; i++)
		{
			BlockTransaction &t = block.transactions[i];
			mTotalInputCount+=t.inputCount;
			mTotalOutputCount+=t.outputCount;
			for (uint32_t j=0; j<t.inputCount; j++)
			{
				BlockInput &input = t.inputs[j];
				if ( input.transactionIndex != 0xFFFFFFFF )
				{
					Hash256 thash(input.transactionHash);
					FileLocation key(thash,0,0,0,0);
					FileLocation *found = mTransactionMap.find(key);
					if ( found == NULL )
					{
						block.warning = true;
					}
				}
			}
		}
	}


	virtual const Block *readBlock(uint32_t blockIndex)
	{
		Block *ret = NULL;

		if ( readBlock(mSingleBlock,blockIndex) )
		{
			ret = &mSingleBlock;
		}

		return ret;
	}

	virtual bool readBlock(BlockImpl &block,uint32_t blockIndex)
	{
		bool ret = false;

		if ( blockIndex >= mBlockCount ) return false;
		BlockHeader &header = *mBlockHeaders[blockIndex];
		FILE *fph = mBlockChain[header.mFileIndex];
		if ( fph )
		{
			block.blockIndex = blockIndex;
			block.warning = false;
			fseek(fph,header.mFileOffset,SEEK_SET);
			gBlockIndex = blockIndex;
			block.blockLength = header.mBlockLength;
			block.blockReward = 0;
			block.totalInputCount = 0;
			block.totalOutputCount = 0;
			block.fileIndex = header.mFileIndex;
			block.fileOffset = header.mFileOffset;
			block.blockLength = header.mBlockLength;

			if ( blockIndex < (mBlockCount-2) )
			{
				BlockHeader *nextNext = mBlockHeaders[blockIndex+2];
				block.nextBlockHash =  nextNext->mPreviousBlockHash;
			}

			uint8_t *blockData = mBlockDataBuffer;
			size_t r = fread(blockData,block.blockLength,1,fph); // read the rest of the block (less the 8 byte header we have already consumed)
			if ( r == 1 )
			{
				BLOCKCHAIN_SHA256::computeSHA256(blockData,4+32+32+4+4+4,block.computedBlockHash);
				BLOCKCHAIN_SHA256::computeSHA256(block.computedBlockHash,32,block.computedBlockHash);
				ret = block.processBlockData(blockData,block.blockLength,mTransactionCount);
				if ( ret )
				{
					processTransactions(block);
				}
			}
			else
			{
				printf("Failed to read input block.  BlockChain corrupted.\r\n");
			}
		}
		block.warning = gIsWarning;
		gIsWarning = false;
		return ret;
	}

	void printReverseHash(const uint8_t *hash)
	{
		if ( hash )
		{
			for (uint32_t i=0; i<32; i++)
			{
				printf("%02x", hash[31-i] );
			}
		}
		else
		{
			printf("NULL HASH");
		}
	}



	virtual void printBlock(const Block *block) // prints the contents of the block to the console for debugging purposes
	{
		printf("==========================================================================================\r\n");
		printf("Block #%s\r\n", formatNumber(block->blockIndex) );

		printf("ComputedBlockHash: ");
		printReverseHash(block->computedBlockHash);
		printf("\r\n");

		if ( block->previousBlockHash )
		{
			printf("PreviousBlockHash:");
			printReverseHash(block->previousBlockHash);
			printf("\r\n");
		}
		if ( block->nextBlockHash )
		{
			printf("NextBlockHash:");
			printReverseHash(block->nextBlockHash);
			printf("\r\n");
		}


		printf("Merkle root: ");
		printReverseHash(block->merkleRoot);
		printf("\r\n");

		printf("Number of Transactions: %s\r\n", formatNumber(block->transactionCount) );
		printf("Timestamp : %s\r\n", getTimeString(block->timeStamp ) );
		printf("Bits: %d Hex: %08X\r\n", block->bits, block->bits );
		printf("Size: %0.10f KB or %s bytes.\r\n", (float)block->blockLength / 1024.0f, formatNumber(block->blockLength) );
		printf("Version: %d\r\n", block->blockFormatVersion );
		printf("Nonce: %u\r\n", block->nonce );
		printf("BlockReward: %f\r\n", (float)block->blockReward / ONE_BTC );

		printf("%s transactions\r\n", formatNumber(block->transactionCount) );
		for (uint32_t i=0; i<block->transactionCount; i++)
		{
			const BlockTransaction &t = block->transactions[i];
			printf("Transaction %s : %s inputs %s outputs. VersionNumber: %d\r\n", formatNumber(i), formatNumber(t.inputCount), formatNumber(t.outputCount), t.transactionVersionNumber );
			printf("TransactionHash: ");
			printReverseHash(t.transactionHash);
			printf("\r\n");
			for (uint32_t i=0; i<t.inputCount; i++)
			{
				const BlockInput &input = t.inputs[i];
				printf("    Input %s : ResponsScriptLength: %s TransactionIndex: %s : TransactionHash: ", formatNumber(i), formatNumber(input.responseScriptLength), formatNumber(input.transactionIndex) );

				printReverseHash(input.transactionHash);

				printf("\r\n");

				if ( input.transactionIndex != 0xFFFFFFFF )
				{
					const BlockTransaction *t = readSingleTransaction(input.transactionHash);
					if ( t == NULL )
					{
						printf("ERROR: TransactionIndex[%d] FAILED TO LOCATE TRANSACTION FOR HASH: ", input.transactionIndex );
						printReverseHash(input.transactionHash);
						printf("\r\n");
					}
					else
					{
						if ( input.transactionIndex < t->outputCount )
						{
							const BlockOutput &o = t->outputs[input.transactionIndex];
							if ( o.publicKey )
							{
								char scratch[256];
								bool ok=true;
								if ( o.isRipeMD160 )
								{
									uint8_t temp[25];
									BLOCKCHAIN_BITCOIN_ADDRESS::bitcoinRIPEMD160ToAddress(o.publicKey,temp);
									BLOCKCHAIN_BITCOIN_ADDRESS::bitcoinAddressToAscii(temp,scratch,256);
								}
								else
								{
									ok = BLOCKCHAIN_BITCOIN_ADDRESS::bitcoinPublicKeyToAscii(o.publicKey,scratch,256);
								}
								if ( ok )
								{
									printf("     Spending From Public Key: %s in the amount of: %0.4f\r\n", scratch, (float)o.value / ONE_BTC );
								}
								else
								{
									printf("ERROR: Unable to decode public key!\r\n");
								}
							}
							else
							{
								printf("ERROR: No public key found for this previous output.\r\n");
							}
						}
						else
						{
							printf("ERROR: Invalid transaction index!\r\n");
						}
					}
				}
			}
			for (uint32_t i=0; i<t.outputCount; i++)
			{
				const BlockOutput &output = t.outputs[i];
				printf("    Output: %s : %f BTC : ChallengeScriptLength: %s\r\n", formatNumber(i), (float)output.value / ONE_BTC, formatNumber(output.challengeScriptLength) );
				if ( output.publicKey )
				{
					char scratch[256];
					bool ok=true;
					if ( output.isRipeMD160 )
					{
						uint8_t temp[25];
						BLOCKCHAIN_BITCOIN_ADDRESS::bitcoinRIPEMD160ToAddress(output.publicKey,temp);
						BLOCKCHAIN_BITCOIN_ADDRESS::bitcoinAddressToAscii(temp,scratch,256);
					}
					else
					{
						ok = BLOCKCHAIN_BITCOIN_ADDRESS::bitcoinPublicKeyToAscii(output.publicKey,scratch,256);
					}
					if ( ok )
					{
						printf("PublicKey: %s : %s\r\n", scratch, output.isRipeMD160 ? "RIPEMD160" : "ECDSA" );
					}
					else
					{
						printf("Failed to encode the public key.\r\n");
					}
				}
				else
				{
					printf("ERROR: Unable to derive a public key for this output!\r\n");
				}
			}
		}

		printf("==========================================================================================\r\n");
	}

	virtual const Block * processSingleBlock(const void *blockData,uint32_t blockLength) 
	{
		const Block *ret = NULL;
		if ( blockLength < MAX_BLOCK_SIZE )
		{
			mSingleBlock.blockIndex = 0;
			mSingleBlock.blockReward = 0;
			mSingleBlock.totalInputCount = 0;
			mSingleBlock.totalOutputCount = 0;
			mSingleBlock.fileIndex = 0;
			mSingleBlock.fileOffset =  0;
			uint32_t transactionIndex=0;
			mSingleBlock.processBlockData(blockData,blockLength,transactionIndex);
			ret = static_cast< Block *>(&mSingleBlock);
		}
		return ret;
	}

	virtual const BlockTransaction *processSingleTransaction(const void *transactionData,uint32_t transactionLength)
	{
		const BlockTransaction *ret = NULL;
		if ( transactionLength < MAX_BLOCK_SIZE )
		{
			mSingleBlock.blockIndex = 0;
			mSingleBlock.blockReward = 0;
			mSingleBlock.totalInputCount = 0;
			mSingleBlock.totalOutputCount = 0;
			mSingleBlock.fileIndex = 0;
			mSingleBlock.fileOffset =  0;
			ret = mSingleBlock.processTransactionData(transactionData,transactionLength);
		}
		return ret;

	}

	virtual const BlockTransaction *readSingleTransaction(const uint8_t *transactionHash) 
	{
		const BlockTransaction *ret = NULL;

		Hash256 h(transactionHash);
		FileLocation key(h,0,0,0,0);
		FileLocation *found = mTransactionMap.find(key);
		if ( found == NULL )
		{
			printf("ERROR: Unable to locate this transaction hash:");
			printReverseHash(transactionHash);
			printf("\r\n");
			return NULL; 
		}
		const FileLocation &f = *found;
		uint32_t fileIndex = f.mFileIndex;
		uint32_t fileOffset = f.mFileOffset;
		uint32_t transactionLength = f.mFileLength;

		if ( fileIndex < MAX_BLOCK_FILES && mBlockChain[fileIndex] && transactionLength < MAX_BLOCK_SIZE )
		{
			FILE *fph = mBlockChain[fileIndex];
			uint32_t saveLocation = (uint32_t)ftell(fph);
			fseek(fph,fileOffset,SEEK_SET);
			uint32_t s = (uint32_t)ftell(fph);
			if ( s == fileOffset )
			{
				uint8_t *blockData = mTransactionBlockBuffer;
				size_t r = fread(blockData,transactionLength,1,fph);
				if ( r == 1 ) // if we successfully read in the entire transaction
				{
					ret = processSingleTransaction(blockData,transactionLength);
					if ( ret )
					{
						BlockTransaction *t = (BlockTransaction *)ret;
						t->transactionIndex = f.mTransactionIndex;
						t->fileIndex = fileIndex;
						t->fileOffset = fileOffset;
					}
				}
				else
				{
					assert(0);
				}
			}
			else
			{
				assert(0);
			}
			fseek(fph,saveLocation,SEEK_SET); // restore the file position back to it's previous location.
		}
		else
		{
			assert(0);
		}
		return ret;
	}

	virtual void processTransactions(const Block *block) // process the transactions in this block and assign them to individual wallets
	{
		if ( !block ) return;

		Transaction *transactions = mTransactionFactory.getTransactions(block->transactionCount);
		if ( !transactions ) return;

		mTransactionFactory.markBlock(transactions);

		for (uint32_t i=0; i<block->transactionCount; i++)
		{

			const BlockTransaction &t = block->transactions[i];
			Transaction &trans = transactions[i];
			trans.mBlock = block->blockIndex;
			trans.mTime = block->timeStamp;
			trans.mInputCount = t.inputCount;
			trans.mOutputCount = t.outputCount;

			trans.mInputs   = mTransactionFactory.getInputs(trans.mInputCount);
			trans.mOutputs  = mTransactionFactory.getOutputs(trans.mOutputCount);

			if ( trans.mInputs == NULL || trans.mOutputs == NULL )
			{
				break;
			}

			for (uint32_t i=0; i<t.outputCount; i++)
			{
				const BlockOutput &output = t.outputs[i];
				TransactionOutput &to = trans.mOutputs[i];

				uint32_t adr = 0;

				if ( output.publicKey )
				{
					if ( output.isRipeMD160 )
					{
						mTransactionFactory.getAddress(output.publicKey,adr);
					}
					else
					{
						uint8_t address[25];
						BLOCKCHAIN_BITCOIN_ADDRESS::bitcoinPublicKeyToAddress(output.publicKey,address);
						mTransactionFactory.getAddress(&address[1],adr);
					}
				}
				to.mAddress = adr;
				to.mValue = output.value;
			}

			for (uint32_t i=0; i<t.inputCount; i++)
			{
				const BlockInput &input = t.inputs[i];
				TransactionInput &tin = trans.mInputs[i];
				tin.mOutput = NULL;

				if ( input.transactionIndex != 0xFFFFFFFF )
				{
					Hash256 h(input.transactionHash);
					FileLocation key(h,0,0,0,0);
					FileLocation *found = mTransactionMap.find(key);
					assert(found);
					if ( found )
					{
						Transaction *previousTransaction = mTransactionFactory.getSingleTransaction(found->mTransactionIndex);
						if ( previousTransaction == NULL )
						{
							printf("ERROR: FAILED TO LOCATE TRANSACTION!\r\n");
						}
						else
						{
							assert( input.transactionIndex < previousTransaction->mOutputCount );
							if ( input.transactionIndex < previousTransaction->mOutputCount )
							{
								tin.mOutput = &previousTransaction->mOutputs[input.transactionIndex];
								mTransactionFactory.getAddress(tin.mOutput->mAddress);
							}
						}
					}
				}
			}
		}
	}

	virtual uint32_t gatherAddresses(void)
	{
		mTransactionFactory.gatherAddresses();
		return mTransactionFactory.getAddressCount();
	}

	virtual void reportCounts(void)
	{
		printf("Total Blocks: %s\r\n", formatNumber(mBlockCount) );
		printf("Total Transactions: %s\r\n", formatNumber(mTotalTransactionCount));
		printf("Total Inputs: %s\r\n", formatNumber(mTotalInputCount));
		printf("Total Outputs: %s\r\n", formatNumber(mTotalOutputCount));
		mTransactionFactory.reportCounts();
	}

	virtual void printTransactions(uint32_t blockIndex)
	{
		mTransactionFactory.printTransactions(blockIndex);
	}

	virtual void gatherStatistics(uint32_t stime,uint32_t zombieDate,bool record_addresses)
	{
		mTransactionFactory.gatherStatistics(stime,zombieDate,record_addresses);
	}

	virtual void saveStatistics(bool record_addresses)
	{
		mTransactionFactory.saveStatistics(record_addresses);
	}

	virtual void printAddresses(void) 
	{
		mTransactionFactory.printAddresses();
	}

	virtual void printBlocks(void)
	{
		for (uint32_t i=0; i<mBlockCount; i++)
		{
			mTransactionFactory.printTransactions(i);
		}
	}

	bool readBlockHeader(void)
	{
		bool ok = false;
		FILE *fph = mBlockChain[mBlockIndex];
		if ( fph )
		{
			uint32_t magicID = 0;
			uint32_t lastBlockRead = (uint32_t)ftell(fph);
			size_t r = fread(&magicID,sizeof(magicID),1,fph);	// Attempt to read the magic id for the next block
			if ( r == 0 )
			{
				mBlockIndex++;	// advance to the next data file if we couldn't read any further in the current data file
				if ( openBlock() )
				{
					fph = mBlockChain[mBlockIndex];
					r = fread(&magicID,sizeof(magicID),1,fph); // if we opened up a new file; read the magic id from it's first block.
					lastBlockRead = ftell(fph);
				}
			}
			// If after reading the previous block, we did not encounter a block header, we need to scan for the next block header..
			if ( r == 1 && magicID != MAGIC_ID )
			{
				fseek(fph,lastBlockRead,SEEK_SET);
				printf("Warning: Missing block-header; scanning for next one.\r\n");
				uint8_t *temp = (uint8_t *)::malloc(MAX_BLOCK_SIZE);
				memset(temp,0,MAX_BLOCK_SIZE);
				uint32_t c = (uint32_t)fread(temp,1,MAX_BLOCK_SIZE,fph);
				bool found = false;
				if ( c > 0 )
				{
					for (uint32_t i=0; i<c; i++)
					{
						const uint32_t *check = (const uint32_t *)&temp[i];
						if ( *check == MAGIC_ID )
						{
							printf("Found the next block header after skipping: %s bytes forward in the file.\r\n", formatNumber(i) );
							lastBlockRead+=i; // advance to this location.
							found = true;
							break;
						}
					}
				}
				::free(temp);
				if ( found )
				{
					fseek(fph,lastBlockRead,SEEK_SET);
					r = fread(&magicID,sizeof(magicID),1,fph); // if we opened up a new file; read the magic id from it's first block.
					assert( magicID == MAGIC_ID );
				}

				if ( found ) // if we found it before the EOF, we are cool, otherwise, we need to advance to the next file.
				{
				}
				else
				{
					mBlockIndex++;	// advance to the next data file if we couldn't read any further in the current data file
					if ( openBlock() )
					{
						fph = mBlockChain[mBlockIndex];
						r = fread(&magicID,sizeof(magicID),1,fph); // if we opened up a new file; read the magic id from it's first block.
						if ( r == 1 )
						{
							if ( magicID != MAGIC_ID )
							{
								printf("Advanced to the next data file; but it does not start with a valid block.  Aborting reading the block-chain.\r\n");
								r = 0;
							}
						}
					}
					else
					{
						r = 0; // done
					}
				}
			}
			if ( r == 1 )	// Ok, this is a valid block, let's continue
			{
				BlockHeader header;
				BlockPrefix prefix;
				header.mFileIndex = mBlockIndex;
				r = fread(&header.mBlockLength,sizeof(header.mBlockLength),1,fph); // read the length of the block
				header.mFileOffset = (uint32_t)ftell(fph);
				if ( r == 1 )
				{
					assert( header.mBlockLength < MAX_BLOCK_SIZE ); // make sure the block length does not exceed our maximum expected ever possible block size
					if ( header.mBlockLength < MAX_BLOCK_SIZE )
					{
						r = fread(&prefix,sizeof(prefix),1,fph); // read the rest of the block (less the 8 byte header we have already consumed)
						if ( r == 1 )
						{
							Hash256 *blockHash = static_cast< Hash256 *>(&header);
							memcpy(header.mPreviousBlockHash,prefix.mPreviousBlock,32);
							BLOCKCHAIN_SHA256::computeSHA256((uint8_t *)&prefix,sizeof(prefix),(uint8_t *)blockHash);
							BLOCKCHAIN_SHA256::computeSHA256((uint8_t *)blockHash,32,(uint8_t *)blockHash);
							uint32_t currentFileOffset = ftell(fph); // get the current file offset.
							uint32_t advance = header.mBlockLength - sizeof(BlockPrefix);
							currentFileOffset+=advance;
							fseek(fph,currentFileOffset,SEEK_SET); // skip past the block to get to the next header.
							mLastBlockHeader = mBlockHeaderMap.insert(header);
							ok = true;
						}
					}
				}
			}
		}
		return ok;
	}

	virtual uint32_t getBlockCount(void) const 
	{
		return mBlockCount; 
	}

	virtual void printBlockHeaders(void) 
	{
		for (uint32_t i=0; i<mBlockCount; i++)
		{
			BlockHeader &h = *mBlockHeaders[i];
			printf("Block #%d : prevBlockHash:", i );
			printReverseHash(h.mPreviousBlockHash);
			printf("\r\n");
		}
	}

	virtual uint32_t buildBlockChain(void) 
	{
		if ( mScanCount )
		{

			printf("Found %s block headers total. %s in the last block.\r\n",
				formatNumber(mBlockHeaderMap.size()),
				formatNumber( mBlockHeaderMap.size()-mLastBlockHeaderCount) );
			printf("Building complete block-chain.\r\n");
			// need to count the total number of blocks...

			if ( mLastBlockHeader )
			{
				mBlockCount = 0;
				const BlockHeader *scan = mLastBlockHeader;
				while ( scan )
				{
					Hash256 prevBlock(scan->mPreviousBlockHash);
					scan = mBlockHeaderMap.find(prevBlock);
					mBlockCount++;
				}
				printf("Found %s blocks and skipped %s orphan blocks.\r\n", formatNumber(mBlockCount), formatNumber(mBlockHeaderMap.size()-mBlockCount));
				mBlockHeaders = new BlockHeader *[mBlockCount];
				uint32_t index = mBlockCount-1;
				scan = mLastBlockHeader;
				while ( scan )
				{
					mBlockHeaders[index] = (BlockHeader *)scan;
					Hash256 prevBlock(scan->mPreviousBlockHash);
					scan = mBlockHeaderMap.find(prevBlock);
					index--;
				}

			}
			mScanCount = 0;
		}

		return mBlockCount;
	}

	virtual bool readBlockHeaders(uint32_t maxBlock,uint32_t &blockCount)
	{
		if ( readBlockHeader() && mScanCount < maxBlock )
		{
			mScanCount++;
			blockCount = mScanCount;
			return true;	// true means there are more blocks to read..
		}

		return false;
	}

	virtual void printAddress(const char *address) 
	{
		mTransactionFactory.printAddress(address);
	}

	virtual void printTopBalances(uint32_t tcount,uint32_t minBalance) 
	{
		mTransactionFactory.printTopBalances(tcount,minBalance);
	}

	virtual void printOldest(uint32_t tcount,uint32_t minBalance) 
	{
		mTransactionFactory.printOldest(tcount,minBalance);
	}

	virtual void zombieReport(uint32_t zdays,uint32_t minBalance)
	{
		mTransactionFactory.zombieReport(zdays,minBalance);
	}

	char						mRootDir[512];					// The root directory name where the block chain is stored
	FILE						*mBlockChain[MAX_BLOCK_FILES];	// The FILE pointer reading from the current file in the blockchain
	uint32_t					mBlockIndex;					// Which index number of the block-chain file sequence we are currently reading.


	size_t						mFileLength;
	uint8_t						mBlockHash[32];	// The current blocks hash


	uint32_t					mBlockBase;	// which index we are pulling working blocks from next
	BlockImpl					mSingleBlock;

	uint32_t					mReadCount;
	uint8_t						*mCurrentBlockData;
	uint8_t						mBlockDataBuffer[MAX_BLOCK_SIZE];	// Holds one block of data
	uint8_t						mTransactionBlockBuffer[MAX_BLOCK_SIZE];
	uint32_t					mTransactionCount;
	TransactionHashMap			mTransactionMap;	// A hash map to the seek file location of all transactions (by hash)
	uint32_t					mLastBlockHeaderCount;

	uint32_t					mTotalTransactionCount;
	uint32_t					mTotalInputCount;
	uint32_t					mTotalOutputCount;
	uint32_t					mScanCount;
	uint32_t					mBlockCount;
	BlockHeader					*mLastBlockHeader;
	BlockHeader					**mBlockHeaders;
	BlockHeaderMap				mBlockHeaderMap;		// A hash-map of all of the block headers
	BitcoinTransactionFactory	mTransactionFactory;	// the factory that accumulates all transactions on a per-address basis

};


BlockChain *createBlockChain(const char *rootPath)
{
	BlockChainImpl *b = new BlockChainImpl(rootPath);
	if ( !b->isValid() )
	{
		delete b;
		b = NULL;
	}
	return static_cast<BlockChain *>(b);
}
