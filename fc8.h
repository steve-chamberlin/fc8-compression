/*
* FC8 compression by Steve Chamberlin
* Derived from liblzg by Marcus Geelnard
*/

#ifndef _FC8_H_
#define _FC8_H_

#define FC8_HEADER_SIZE 8
#define FC8_DECODED_SIZE_OFFSET 4

uint32_t Encode(const uint8_t *in, uint32_t insize, uint8_t *out, uint32_t outsize);

uint32_t Decode(const uint8_t *in, uint32_t insize, uint8_t *out, uint32_t outsize);

uint32_t GetDecodedSize(const uint8_t *in);

#endif // _FC8_H_
