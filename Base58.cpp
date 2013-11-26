#include "Base58.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#ifdef _MSC_VER // Disable the stupid ass absurd warning messages from Visual Studio telling you that using stdlib and stdio is 'not valid ANSI C'
#pragma warning(disable:4718)
#pragma warning(disable:4996)
#endif


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
					   bool isBigEndian,		 // True if the input number is in little-endian format (this will be true for a bitcoin address)
					   char *output,			 // The address to store the output string.
					   uint32_t maxStrLen)		 // the maximum length of the output string
{
	// Before passing the hash into the base58 encoder; we need to reverse the byte order.
	uint8_t hash[25];
	if ( isBigEndian )
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
					   bool isBigEndian) // If the output needs to be in little endian format
{
	uint32_t ret = 0;

	BigNumber bn(NULL,0);
	if ( CBDecodeBase58(&bn,string) && bn.length <= maxOutputLength )
	{
		ret = bn.length;
		if ( isBigEndian ) // if the caller wants the number returned in little endian format; then we byte reverse the output
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