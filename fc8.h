/*
* FC8 compression by Steve Chamberlin
* Derived from liblzg by Marcus Geelnard
*/

#ifndef _FC8_H_
#define _FC8_H_

#define FC8_HEADER_SIZE 8
#define FC8_DECODED_SIZE_OFFSET 4

// for FC8b block format header
#define FC8_BLOCK_HEADER_SIZE 12
#define FC8_BLOCK_SIZE_OFFSET 8

uint32_t Encode(const uint8_t *in, uint32_t insize, uint8_t *out, uint32_t outsize);

uint32_t Decode(const uint8_t *in, uint32_t insize, uint8_t *out, uint32_t outsize);

uint32_t GetUInt32(const uint8_t *in);
void SetUInt32(uint8_t *in, uint32_t val);

#endif // _FC8_H_
