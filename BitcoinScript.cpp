#include "BitcoinScript.h"
#include <assert.h>
#include <string.h>

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

#define MAX_STACK_SIZE 2048	// maximum size of the stack
#define MAX_STACK_ELEMENTS 64	// never expect more than 64 elements to be pushed onto the stack

class BitcoinScriptImpl : public BitcoinScript
{
public:
	// A simple stack class for processing bitcoin scripts
	class Stack
	{
	public:
		Stack(void)
		{
			resetStack();
		}

		void resetStack(void)
		{
			mStackPtr8 = mStack;
			mStackIndex = 0;
		}

		void push(uint8_t size,const uint8_t *&script)
		{
			mStackElements[mStackIndex] = mStackPtr8;
			mStackSize[mStackIndex] = size;
			mStackIndex++;
			assert( mStackIndex < MAX_STACK_ELEMENTS );
			switch ( size )
			{
				case 0:
					// it is considered valid to push zero bytes onto the stack; essentially just increments the stack pointer
					break;
				case 1:
					*mStackPtr8++ = *script;	// copy one byte
					break;
				case 2:
					*mStackPtr16++ = *script; // copy two bytes
					break;
				case 4:
					*mStackPtr32++ = *script;	// copy 4 bytes
					break;
				case 8:
					*mStackPtr64++ = *script;	// copy 8 bytes
					break;
				default:
					memcpy(mStackPtr8,script,size);
					mStackPtr8+=size;
					break;
			}
			script+=size;	// advance the script intput instruction pointer.
			assert(mStackPtr8 < &mStack[MAX_STACK_SIZE]);	// assert that we have not had a stack overflow
		}

		inline uint32_t getStackSize(void) const
		{
			return mStackIndex;
		}

		inline const uint8_t *getStackEntry(uint32_t offset,uint8_t &size) const
		{
			assert( offset <= mStackIndex );
			offset = mStackIndex-offset;
			size = mStackSize[offset];
			return mStackElements[offset];
		}

		union
		{
		uint8_t *mStackPtr8;			// current stack pointer as U8
		uint16_t *mStackPtr16;			// current stack pointer as U16
		uint32_t *mStackPtr32;			// current stack pointer.as U32
		uint64_t *mStackPtr64;			// current stack pointer.as U64
		};
		uint8_t	mStack[MAX_STACK_SIZE];
		uint32_t	mStackIndex;
		uint8_t		*mStackElements[MAX_STACK_ELEMENTS];
		uint8_t		mStackSize[MAX_STACK_ELEMENTS];
	};

	BitcoinScriptImpl(void)
	{
		mLastError = NULL;
	}
	~BitcoinScriptImpl(void)
	{

	}

	virtual bool executeScript(const uint8_t *script,uint32_t scriptLength)
	{
		bool ret = true;

		mLastError = NULL; // clear the last error state
		const uint8_t *endOfScript = script+scriptLength;
		while ( script < endOfScript && ret )
		{
			uint8_t opcode = *script++;
			if ( opcode < OP_PUSHDATA1 )	// Any opcode less than OP_PUSHDATA1 is simply an instruction to push that many bytes onto the stack
			{
				mStack.push(opcode,script);
			}
			else
			{
				switch	( opcode )
				{
					case OP_0:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_PUSHDATA1:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_PUSHDATA2:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_PUSHDATA4:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_RESERVED:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_1NEGATE:
					case OP_1:
					case OP_2:
					case OP_3:
					case OP_4:
					case OP_5:
					case OP_6:
					case OP_7:
					case OP_8:
					case OP_9:
					case OP_10:
					case OP_11:
					case OP_12:
					case OP_13:
					case OP_14:
					case OP_15:
					case OP_16:
						{
							//int value = (int)opcode - (int)(OP_1 - 1);
							assert(0); // Not yet implemented
						}
						break;
					case OP_NOP:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_VER:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_IF:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_NOTIF:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_VERIF:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_VERNOTIF:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_ELSE:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_ENDIF:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_VERIFY:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_RETURN:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_TOALTSTACK:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_FROMALTSTACK:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_2DROP:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_2DUP:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_3DUP:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_2OVER:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_2ROT:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_2SWAP:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_IFDUP:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_DEPTH:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_DROP:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_DUP:
						{
							if ( mStack.getStackSize() < 1 )
							{
								ret = false;
								mLastError = "OP_DUP : Nothing on the stack to duplicate";
							}
							else
							{
								uint8_t len;
								const uint8_t *val = mStack.getStackEntry(1,len);
								mStack.push(len,val);
							}
						}
						break;
					case OP_NIP:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_OVER:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_PICK:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_ROLL:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_ROT:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_SWAP:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_TUCK:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_SIZE:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_CAT:
					case OP_SUBSTR:
					case OP_LEFT:
					case OP_RIGHT:
					case OP_INVERT:
					case OP_AND:
					case OP_OR:
					case OP_XOR:
					case OP_2MUL:
					case OP_2DIV:
					case OP_MUL:
					case OP_DIV:
					case OP_MOD:
					case OP_LSHIFT:
					case OP_RSHIFT:
						{
							mLastError = "Encountered a disabled opcode";
							ret = true;
						}
						break;
					case OP_EQUAL:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_EQUALVERIFY:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_RESERVED1:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_RESERVED2:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_1ADD:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_1SUB:
						{
							assert(0); // Not yet implemented
						}
						break;

					case OP_NEGATE:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_ABS:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_NOT:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_0NOTEQUAL:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_ADD:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_SUB:
						{
							assert(0); // Not yet implemented
						}
						break;

					case OP_BOOLAND:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_BOOLOR:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_NUMEQUAL:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_NUMEQUALVERIFY:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_NUMNOTEQUAL:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_LESSTHAN:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_GREATERTHAN:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_LESSTHANOREQUAL:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_GREATERTHANOREQUAL:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_MIN:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_MAX:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_WITHIN:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_RIPEMD160:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_SHA1:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_SHA256:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_HASH160:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_HASH256:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_CODESEPARATOR:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_CHECKSIG:
						{
							if ( mStack.getStackSize() < 2 ) // must be two items on the stack!
							{
								ret = false;
								mLastError = "OP_CHECKSIG : Not two data items on the stack.";
							}
							else
							{
								assert(0); // Not yet implemented
							}
						}
						break;
					case OP_CHECKSIGVERIFY:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_CHECKMULTISIG:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_CHECKMULTISIGVERIFY:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_NOP1:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_NOP2:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_NOP3:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_NOP4:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_NOP5:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_NOP6:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_NOP7:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_NOP8:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_NOP9:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_NOP10:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_SMALLINTEGER:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_PUBKEYS:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_PUBKEYHASH:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_PUBKEY:
						{
							assert(0); // Not yet implemented
						}
						break;
					case OP_INVALIDOPCODE:
						{
							assert(0); // Not yet implemented
						}
						break;
					default:
						{
							assert(0); // invalid opcode!
						}
						break;
				}
			}
		}
		return ret;
	}

	virtual void resetStack(void) 
	{
		mStack.resetStack();
	}

	virtual void release(void)
	{
		delete this;
	}

	virtual const char * getLastError(void) const
	{
		return mLastError;
	}

	const char *mLastError;
	Stack	mStack;	// the script virtual machine stack
};

BitcoinScript *createBitcoinScript(void)
{
	BitcoinScriptImpl *b = new BitcoinScriptImpl;
	return static_cast< BitcoinScript *>(b);
}
