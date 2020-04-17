
#include "fibbits.h"
#include <stdio.h>

bool testGetBits() {
  uint8_t bitPerByte[16 + 8 + 32];
  uint8_t bitsPerBit[3 + 4];
  int ne = 0;
  uint16_t v;

  for (int k = 0; k < 16 + 8 + 32; ++k) bitPerByte[k] = 0;
  for (int k = 0; k < 3 + 4; ++k) bitsPerBit[k] = 0;
  for (v = 1U; v < 65535U; ++v) {
    for (int k = 0; k < 16; ++k) bitPerByte[k] = (v >> k) & 1;
    bitsPerBit[0] = getBits_bitPerByte(8, 0, bitPerByte);
    bitsPerBit[1] = getBits_bitPerByte(8, 8, bitPerByte);

    for (int offset = 0; offset <= 12; ++offset) {
      uint8_t w4a = getBits_bitPerByte(4, offset, bitPerByte);
      uint8_t w4b = getMax8Bits(4, offset, bitsPerBit);

      if (w4a != w4b) {
        fprintf(stderr, "test error: v = %u, offset %d, w4a %u, w4b %u\n",
                (unsigned)v, offset, (unsigned)w4a, (unsigned)w4b);
        ++ne;
      }
    }

    for (int offset = 0; offset <= 12; ++offset) {
      uint8_t w8a = getBits_bitPerByte(8, offset, bitPerByte);
      uint8_t w8b = getMax8Bits(8, offset, bitsPerBit);
      if (w8a != w8b) {
        fprintf(stderr, "test error: v = %u, offset %d, w8a %u, w8b %u\n",
                (unsigned)v, offset, (unsigned)w8a, (unsigned)w8b);
        ++ne;
      }
    }

    for (int offset = 0; offset <= 12; ++offset) {
      uint16_t w12a = getBits_bitPerByte(12, offset, bitPerByte);
      uint16_t w12b = getMax16Bits(12, offset, bitsPerBit);
      if (w12a != w12b) {
        fprintf(stderr, "test error: v = %u, offset %d, w12a %u, w12b %u\n",
                (unsigned)v, offset, (unsigned)w12a, (unsigned)w12b);
        ++ne;
      }
    }

    for (int offset = 0; offset <= 2; ++offset) {
      uint32_t w32a = getBits_bitPerByte(32, offset, bitPerByte);
      uint32_t w32b = getMax32Bits(32, offset, bitsPerBit);
      if (w32a != w32b) {
        fprintf(stderr, "test error: v = %u, offset %d, w32a %u, w32b %u\n",
                (unsigned)v, offset, (unsigned)w32a, (unsigned)w32b);
        ++ne;
      }
    }
  }
  return (ne == 0);
}
