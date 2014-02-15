#ifndef BITCOIN_ADDRESS_H

#define BITCOIN_ADDRESS_H

// This code snippet can convert a bitcoin public key (65 bytes) into a corresponding bitcoin address (25) bytes.
// It can also convert the binary bitcoin address to and from ASCII; including the appropriate header and checksums as necessary.
// Read some of the following documentation to understand the difference between a bitcoin public key and a bitcoin address.
//
// https://en.bitcoin.it/wiki/Base58Check_encoding
//
// https://en.bitcoin.it/wiki/Technical_background_of_version_1_Bitcoin_addresses
// 
// This code snippet is essentially a reference implementation of that algorithm described above.
//
// This code snippet leverages against code from three other sources.  It uses the base58 encoder/decoder found in the 'cbitcoin' source code.
// It has been heavily refactored to remove all memory allocations.
// It also uses the SHA256 code written by  Zilong Tan (eric.zltan@gmail.com) and released under MIT license.
// Finally it uses the RPIEMD160.cpp hash implementation written by Antoon Bosselaers, ESAT-COSIC
//
////
// http://cbitcoin.com/
//
// If you find this code snippet useful; you can tip me at this bitcoin address:
//
// BITCOIN TIP JAR: "1BT66EoaGySkbY9J6MugvQRhMMXDwPxPya"
//
// This is a bit tricky.  The binary representation of a bitcoin address is based on doing some computations on the public ECDSA key.
// You can learn more about the ECDSA key (Elliptic Curve Digital Signature) https://en.bitcoin.it/wiki/ECDSA and http://en.wikipedia.org/wiki/Elliptic_Curve_DSA
//
// The public portion of the bitcoin ECDSA key is always 65 bytes long; the first byte is a hard coded 0x4 followed by the two 32 bytes of 
// the elliptic curve (32 bytes X) (32 bytes Y)
//
// Here is how this is converted into the 25 byte bitcoin address and subsequence base58 encoding.
//
// Step #1 : Perform SHA-256 hash of these 65 bytes.
// Step #2 : Perform RIPEMD-160 hash on the 32 byte SHA-256 hash producing a 20 byte output.
// Step #3 : Add a 'version' byte of zero to the front of the RIPEMD-160 hash (now it's going to be 21 bytes)
// Step #4 : Perform SHA-256 hash of the previous step (21 bytes to a 32 byte SHA-256 hash)
// Step #5 : Perform SHA-256 hash of the previous hash.
// Step #6 : Take the first 4 bytes of Step #5 and use it as a 4 byte checksum.
// Step #7 : Add the 4 byte checksum to then end of the previous 21 bytes hash at step #3; you should now have 25 bytes.
// Step #8 : Reverse the order of the 25 bytes before passing it into the bignumber routines provided
// Step #9 : Convert the result to ASCII using Base58-Check encoding. https://en.bitcoin.it/wiki/Base58Check_encoding
//
// Another good reference for how this works is this page: http://gobittest.appspot.com/Address
//
// To do this computation, you have to treat the entire input binary address as a single very large number and then keep computing
// it's base58 value until exhausted.  There is no way to do this without also having a math library which can operate on large
// integer numbers.
// 
//
// Converting bitcoin addresses from binary to ASCII and ASCII to binary is generally considered as utility function and not
// subject to full performance considerations.
#include <stdint.h>

// Converts an ASCII bitcoin address into the 25 byte version.
bool bitcoinAsciiToAddress(const char *input,uint8_t output[25]); // convert an ASCII bitcoin address into binary.

// Converts a 25 byte bitcoin address into the ASCII versions
bool bitcoinAddressToAscii(const uint8_t address[25],char *output,uint32_t maxOutputLen);

// Converts a full 65 byte ECDSA public key into an ASCII representation
bool bitcoinPublicKeyToAscii(const uint8_t input[65], // The 65 bytes long ECDSA public key; first byte will always be 0x4 followed by two 32 byte components
						  char *output,				// The output ascii representation.
						  uint32_t maxOutputLen); // convert a binary bitcoin address into ASCII

// Converts a full 65 byte ECDSA public key into the 25 byte RIPEMD160 version (with header and 4 byte checksum at the end).  
bool bitcoinPublicKeyToAddress(const uint8_t input[65], // The 65 bytes long ECDSA public key; first byte will always be 0x4 followed by two 32 byte components
							   uint8_t output[25]);		// A bitcoin address (in binary( is always 25 bytes long.

// If someone gives you a bitcoin address as the already encoded 20 byte RIPEMD, then this will produce the 25 byte 'address' which has the padding and checksum added to it.
// This puts the one byte header and the 4 byte checksum to conver a 20 byte RIPEMD public key into the padded 25 byte address version
void bitcoinRIPEMD160ToAddress(const uint8_t ripeMD160[20],uint8_t address[25]);

#endif
