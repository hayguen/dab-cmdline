
#pragma once

#include <stdint.h>
#include <string.h>

// one uint8_t of d[] contains one single usable bit
static inline
uint32_t getBits_bitPerByte( int num, int32_t offset, const uint8_t *d) {
    uint32_t res = d [offset];
    for (int k = 1; k < num; ++k ) {
        res <<= 1;
        res |= d [offset + k];
    }
    return res;
}

////////////////////////////////////////////////////////////////////////

// one uint8_t of fib[] contains up to 8 usable bits
static inline
uint8_t getSingleBit( int bitStart, const uint8_t * fib )
{
    int byteStart = bitStart / 8;
    int bitLoIdx = 7 - (bitStart & 7);
    return (fib[byteStart] >> bitLoIdx) & 1;
}

//                           1  2  3   4   5   6    7    8    9    10,   11,   12,   13,    14,    15,    16
static uint16_t mask[16] = { 1, 3, 7, 15, 31, 63, 127, 255, 511, 1023, 2047, 4095, 8191, 16383, 32767, 65535 };


// one uint8_t of fib[] contains up to 8 usable bits
static inline
uint8_t getMax8Bits( int num, int bitStart, const uint8_t * fib )
{
    int byteStart = bitStart / 8;
    int bitLoIdx = 7 - ( (bitStart & 7) + num -1 );
    if ( bitLoIdx >= 0 )
        return (fib[byteStart] >> bitLoIdx) & mask[num -1];
    else
    {
        uint16_t w = ( ((uint16_t)fib[byteStart]) << 8 ) | ((uint16_t)fib[byteStart+1]);
        return (w >> (bitLoIdx +8)) & mask[num -1];
    }
    return 0;
}

// one uint8_t of fib[] contains up to 8 usable bits
static inline
uint16_t getMax16Bits( int num, int bitStart, const uint8_t * fib )
{
    int byteStart = bitStart / 8;
    int bitLoIdx = 7 - ( (bitStart & 7) + num -1 );
    if ( bitLoIdx >= 0 )
        return (fib[byteStart] >> bitLoIdx) & mask[num -1];
    else if ( bitLoIdx + 8 >= 0 )
    {
        uint16_t w = ( ((uint16_t)fib[byteStart]) << 8 ) | ((uint16_t)fib[byteStart+1]);
        return (w >> (bitLoIdx +8)) & mask[num -1];
    }
    else if ( bitLoIdx + 16 >= 0 )
    {
        uint32_t w = ( ((uint32_t)fib[byteStart]) << 16 )
                   | ( ((uint32_t)fib[byteStart+1]) << 8 )
                   | ( ((uint32_t)fib[byteStart+2]) );
        return (w >> (bitLoIdx +16)) & mask[num -1];
    }
    return 0;
}

// one uint8_t of fib[] contains up to 8 usable bits
static inline
uint32_t getMax32Bits( int num, int bitStart, const uint8_t * fib )
{
    if ( num <= 16 )
        return getMax16Bits( num, bitStart, fib );
    uint32_t hi = getMax16Bits( num-16, bitStart, fib );
    uint32_t lo = getMax16Bits( 16, bitStart+num-16, fib );
    uint32_t t = (hi << 16) | lo;
    return t;
}


static inline
bool	check_crc_bytes (const uint8_t *msg, int32_t len) {
int i, j;
uint16_t	accumulator	= 0xFFFF;
uint16_t	crc;
uint16_t	genpoly		= 0x1021;

	for (i = 0; i < len; i ++) {
	   int16_t data = msg [i] << 8;
	   for (j = 8; j > 0; j--) {
	      if ((data ^ accumulator) & 0x8000)
	         accumulator = ((accumulator << 1) ^ genpoly) & 0xFFFF;
	      else
	         accumulator = (accumulator << 1) & 0xFFFF;
	      data = (data << 1) & 0xFFFF;
	   }
	}
//
//	ok, now check with the crc that is contained
//	in the au
	crc	= ~((msg [len] << 8) | msg [len + 1]) & 0xFFFF;
	return (crc ^ accumulator) == 0;
}

#if 0
static inline
bool	check_CRC_bits (const uint8_t *bin, int32_t size)
{
	static const uint8_t crcPolynome[] =
		{0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0};	// MSB .. LSB
const int32_t dataSize = size - 16;
int32_t	i, f;
uint8_t	b[16];
int16_t	Sum	= 0;

	memset(b, 1, 16);

	for (i = 0; i < size; i++)
	{
	   const uint8_t inbit = getSingleBit( i, bin ) ^ ( i >= dataSize ? 1U : 0U );
	   if ( b[0] ^ inbit )
	   {
	      for (f = 0; f < 15; f++) 
	         b[f] = crcPolynome[f] ^ b[f + 1];
	      b[15] = 1;
	   }
	   else
	   {
	      memmove (&b [0], &b[1], sizeof(uint8_t ) * 15); // Shift
	      b[15] = 0;
	   }
	}

	for (i = 0; i < 16; i++)
	   Sum += b[i];

	return Sum; // == 0;
}

#elif 1

static inline
bool	check_CRC_bits( const uint8_t *in, int32_t size )
{
	static const uint16_t crcPolynome = ( 1U << 3 ) | ( 1U << 10 );
	const int32_t dataSize = size - 16;
	uint16_t b = 0xFFFF;

	// process data bits - except last 16 CRC bits
	if ( !(size & 7) )
	{
	   const int32_t sb = (size / 8) -2;
	   for ( int32_t i = 0; i < sb; ++i )
	   {
	      // https://graphics.stanford.edu/~seander/bithacks.html#ReverseByteWith64BitsDiv
	      uint8_t inByte = (in[i] * 0x0202020202ULL & 0x010884422010ULL) % 1023;  // reverse bits ..

	      b = ((b ^ inByte) & 1) ? ((crcPolynome ^ (b >> 1)) | 0x8000U) : (b >> 1);  inByte >>= 1;  // 1
	      b = ((b ^ inByte) & 1) ? ((crcPolynome ^ (b >> 1)) | 0x8000U) : (b >> 1);  inByte >>= 1;  // 2
	      b = ((b ^ inByte) & 1) ? ((crcPolynome ^ (b >> 1)) | 0x8000U) : (b >> 1);  inByte >>= 1;  // 3
	      b = ((b ^ inByte) & 1) ? ((crcPolynome ^ (b >> 1)) | 0x8000U) : (b >> 1);  inByte >>= 1;  // 4

	      b = ((b ^ inByte) & 1) ? ((crcPolynome ^ (b >> 1)) | 0x8000U) : (b >> 1);  inByte >>= 1;  // 5
	      b = ((b ^ inByte) & 1) ? ((crcPolynome ^ (b >> 1)) | 0x8000U) : (b >> 1);  inByte >>= 1;  // 6
	      b = ((b ^ inByte) & 1) ? ((crcPolynome ^ (b >> 1)) | 0x8000U) : (b >> 1);  inByte >>= 1;  // 7
	      b = ((b ^ inByte) & 1) ? ((crcPolynome ^ (b >> 1)) | 0x8000U) : (b >> 1);  // 8
	   }
	}
	else
	{
	   for ( int32_t i = 0; i < dataSize; ++i )
	   {
	      const uint8_t inbit = getSingleBit( i, in );
	      b = ((b ^ inbit) & 1) ? ((crcPolynome ^ (b >> 1)) | 0x8000U) : (b >> 1);
	   }
	}

	// process last 16 Bits (=CRC) inverted ( input := in ^ 1 )
	for ( int32_t i = dataSize; i < size; ++i )
	{
	   const uint8_t inbit = getSingleBit( i, in ) ^ 1;
	   b = ((b ^ inbit) & 1) ? ((crcPolynome ^ (b >> 1)) | 0x8000U) : (b >> 1);
	}

	return (b == 0);
}

#endif

