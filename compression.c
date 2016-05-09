/*
* FC8 compression by Steve Chamberlin, 2016
* Some concepts and code derived from liblzg by Marcus Geelnard, 2010
*
* This software is provided 'as-is', without any express or implied
* warranty. In no event will the authors be held liable for any damages
* arising from the use of this software.
*
* Permission is granted to anyone to use this software for any purpose,
* including commercial applications, and to alter it and redistribute it
* freely, subject to the following restrictions:
*
* 1. The origin of this software must not be misrepresented; you must not
*    claim that you wrote the original software. If you use this software
*    in a product, an acknowledgment in the product documentation would
*    be appreciated but is not required.
*
* 2. Altered source versions must be plainly marked as such, and must not
*    be misrepresented as being the original software.
*
* 3. This notice may not be removed or altered from any source
*    distribution.
*/

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "fc8.h"

#define _FC8_MAX_MATCH_LENGTH 256
#define _FC8_WINDOW_SIZE (128L*1024)
#define _FC8_MAX_MATCHES (128L*1024)
#define _FC8_LONGEST_LITERAL_RUN 64

/* LUT for encoding the copy length parameter */
const uint8_t _FC8_LENGTH_ENCODE_LUT[257] = {
    255,255,255,0,1,2,3,4,5,6,7,8,9,10,11,12,         /* 0 - 15 */
    13,14,15,16,17,18,19,20,21,22,23,24,25,26,26,26, /* 16 - 31 */
    26,26,26,27,27,27,27,27,27,27,27,27,27,27,27,27, /* 32 - 47 */
    28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28, /* 48 - 63 */
    28,28,28,28,28,28,28,28,29,29,29,29,29,29,29,29, /* 64 - 79 */
    29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29, /* 80 - 95 */
    29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29, /* 96 - 111 */
    29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29, /* 112 - 127 */
    30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30, /* 128 - 143 */
    30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30, /* 144 - 159 */
    30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30, /* 160 - 175 */
    30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30, /* 176 - 191 */
    30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30, /* 192 - 207 */
    30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30, /* 208 - 223 */
    30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30, /* 224 - 239 */
    30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30, /* 240 - 255 */
    31                                               /* 256 */
};

/* LUT for decoding the copy length parameter */
const uint16_t _FC8_LENGTH_DECODE_LUT[32] = {
    3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,35,48,72,128,256
};

/* LUT for quantizing the match length parameter */
const uint16_t _FC8_LENGTH_QUANT_LUT[257] = {
    0,0,0,3,4,5,6,7,8,9,10,11,12,13,14,15,           /* 0 - 15 */
    16,17,18,19,20,21,22,23,24,25,26,27,28,29,29,29, /* 16 - 31 */
    29,29,29,35,35,35,35,35,35,35,35,35,35,35,35,35, /* 32 - 47 */
    48,48,48,48,48,48,48,48,48,48,48,48,48,48,48,48, /* 48 - 63 */
    48,48,48,48,48,48,48,48,72,72,72,72,72,72,72,72, /* 64 - 79 */
    72,72,72,72,72,72,72,72,72,72,72,72,72,72,72,72, /* 80 - 95 */
    72,72,72,72,72,72,72,72,72,72,72,72,72,72,72,72, /* 96 - 111 */
    72,72,72,72,72,72,72,72,72,72,72,72,72,72,72,72, /* 112 - 127 */
    128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128, /* 128 - 143 */
    128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128, /* 144 - 159 */
    128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128, /* 160 - 175 */
    128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128, /* 176 - 191 */
    128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128, /* 192 - 207 */
    128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128, /* 208 - 223 */
    128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128, /* 224 - 239 */
    128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128, /* 240 - 255 */
    256                                                              /* 256 */
};

uint32_t GetUInt32(const uint8_t *in)
{
    return ((uint32_t)in[0]) << 24 |
            ((uint32_t)in[1]) << 16 |
            ((uint32_t)in[2]) << 8 |
            ((uint32_t)in[3]);
}

void SetUInt32(uint8_t *in, uint32_t val)
{
    in[0] = val >> 24;
    in[1] = val >> 16;
    in[2] = val >> 8;
    in[3] = val;
}

typedef struct {
    uint8_t **backchain;
    uint8_t **mostRecent;
} search_accel_t;

search_accel_t* SearchAccel_Create()
{
    search_accel_t *self;

    /* Allocate memory for the sarch tab object */
    self = (search_accel_t *)malloc(sizeof(search_accel_t));
    if (!self)
        return (search_accel_t*) 0;

    /* Backchain linked lists. Total size is one pointer for each entry in the 
       sliding window. Each entry is a pointer to the previous instance of the same
       3-byte sequence that appears at that point in the sliding window. The end
       of this chain may point to older instances that are no longer within the
       sliding window, so the search function must check for this and terminate. */
    self->backchain = (unsigned char**)calloc(_FC8_WINDOW_SIZE, sizeof(unsigned char *));
    if (!self->backchain)
    {
        free(self);
        return (search_accel_t*) 0;
    }

    /* Most recent occurrence lookup table. For 3 byte keys, 256 ^ 3 = 16 meg
       of entries are required. Each entry is a pointer to the most recent 
       occurence of that 3-byte key sequence in the input string, looking backwards 
       from the current position. */
    self->mostRecent = (unsigned char**)calloc(16L*1024*1024, sizeof(unsigned char *));
    if (!self->mostRecent)
    {
        free(self->backchain);
        free(self);
        return (search_accel_t*) 0;
    }

    return self;
}

void SearchAccel_Destroy(search_accel_t *self)
{
    if (!self)
        return;

    free(self->mostRecent);
    free(self->backchain);
    free(self);
}

void UpdateLastPos(search_accel_t *sa, const uint8_t *first, uint8_t *pos)
{
    uint32_t key = (((uint32_t)pos[0]) << 16) | (((uint32_t)pos[1]) << 8) | ((uint32_t)pos[2]);

    sa->backchain[(pos - first) & (_FC8_WINDOW_SIZE-1)] = sa->mostRecent[key]; 
    sa->mostRecent[key] = pos; 
}

uint32_t GetCompressedSizeForMatch(uint32_t dist, uint32_t length)
{
    // BR0 = 01baaaaa  offset aaaaa, length b+3
    // BR1 = 10bbbaaa'aaaaaaaa   offset aaa'aaaaaaaa, length bbb+3
    // BR2 = 11bbbbba'aaaaaaaa'aaaaaaaa   offset a'aaaaaaaa'aaaaaaaa, length LUT[bbbbb]

    // fits in B0?
    if (dist <= 31 && length <= 4)
    {
        return 1;
    }
    // fits in B1?
    else if (dist <= 0x7FF && length <= 10)
    {
        return 2;
    }
    // fits in B2?
    else if (dist <= 0x1FFFF && length <= 256)
    {
        return 3;
    }

    return 0xFFFFFFFF;
}

static uint32_t FindMatch(search_accel_t *sa, const uint8_t *inputStart, const uint8_t *inputEnd, const uint8_t *curPos, uint8_t symbolCost, uint32_t *matchOffset)
{
    uint32_t matchLength, bestLength = 2, dist, preMatch, maxMatches, win, bestWin = 0;
    uint8_t *prevPos, *curPtr, *prevPtr, *minPos, *endStr;

    *matchOffset = 0;

    /* Minimum search position */
    if ((uint32_t)(curPos - inputStart) >= _FC8_WINDOW_SIZE)
        minPos = (uint8_t*)(curPos - _FC8_WINDOW_SIZE);
    else
        minPos = (uint8_t*)inputStart;

    /* Search string end */
    endStr = (uint8_t*)(curPos + _FC8_MAX_MATCH_LENGTH);
    if (endStr > inputEnd)
      endStr = (uint8_t*)inputEnd;

    /* Previous search position */
    prevPos = sa->backchain[(curPos - inputStart) & (_FC8_WINDOW_SIZE - 1)];

    /* Pre-matched by the acceleration structure */
    preMatch = 3;

    /* Main search loop */
    maxMatches = _FC8_MAX_MATCHES;
    while (prevPos && (prevPos > minPos) && (maxMatches--))
    {
        /* If we don't have a match at bestLength, don't even bother... */
        if (curPos[bestLength] == prevPos[bestLength])
        {
            /* Calculate maximum match length for this offset */
            curPtr = (uint8_t*)curPos + preMatch;
            prevPtr = prevPos + preMatch;
            while (curPtr < endStr && *curPtr == *prevPtr)
            {
                ++curPtr;
                ++prevPtr;
            }
            matchLength = curPtr - curPos;

            /* Quantize length */
            matchLength = _FC8_LENGTH_QUANT_LUT[matchLength];

            dist = (uint32_t)(curPos - prevPos);

            /* Get actual compression win for this match */
            win = matchLength + symbolCost - 1 - GetCompressedSizeForMatch(dist, matchLength);

            /* Best win so far? */
            if (win > bestWin)
            {
                bestWin = win;
                *matchOffset = dist;
                bestLength = matchLength;

                /* Did we find a match that was good enough, or did we reach
                    the end of the buffer (no longer match is possible)? */
                if ((matchLength >= _FC8_MAX_MATCH_LENGTH) || (curPtr >= endStr))
                    break;
            }
        }

        /* Previous search position */
        prevPos = sa->backchain[(prevPos - inputStart) & (_FC8_WINDOW_SIZE - 1)];
    }

    /* Did we get a match that would actually compress? */
    if (bestWin > 0)
        return bestLength;
    else
        return 0;
}

uint32_t Encode(const uint8_t *in, uint32_t insize, uint8_t *out, uint32_t outsize)
{
    uint8_t *src, *inEnd, *dst, *outEnd, symbol;
    uint32_t compressedSize, backrefSize;
    uint32_t length, offset = 0, symbolCost, i;
    uint8_t* pRunLengthByte = NULL;
    uint32_t literalRunLength = 0;
    search_accel_t *sa = (search_accel_t*) 0;

    /* Check arguments */
    if ((!in) || (!out) || (outsize < (FC8_HEADER_SIZE + insize)))
        goto fail;

    /* Initialize search accelerator */
    sa = SearchAccel_Create();
    if (!sa)
        goto fail;

    /* Initialize the byte streams */
    src = (uint8_t *)in;
    inEnd = ((uint8_t *)in) + insize;
    dst = out + FC8_HEADER_SIZE;
    outEnd = out + outsize;

    /* Main compression loop */
    while (src < inEnd)
    {
        /* Get current symbol (don't increment, yet) */
        symbol = *src;

        // are there at least three bytes remaining?
        if (inEnd - src >= 3)
        {
            /* What's the cost for this symbol if we do not compress */
            symbolCost = literalRunLength == 0 ? 2 : 1;

            /* Update search accelerator */
            UpdateLastPos(sa, in, src);
        }
        else
        {
            length = 0;
        }    

        /* Find best history match for this position in the input buffer */
        length = FindMatch(sa, in, inEnd, src, symbolCost, &offset);

        if (length > 0)
        {
            if (literalRunLength)
            {
                // terminate the previous literal run, if any
                *pRunLengthByte = literalRunLength-1;
                literalRunLength = 0;
            }

            // find the compressed size of this (offset,length) backref in the new compression scheme
            backrefSize = GetCompressedSizeForMatch(offset, length);
            if (backrefSize > 3)
                goto fail;
            
            // LIT = 00aaaaaa  next aaaaaa+1 bytes are literals
            // BR0 = 01baaaaa  offset aaaaa, length b+3
            // BR1 = 10bbbaaa'aaaaaaaa   offset aaa'aaaaaaaa, length bbb+3
            // BR2 = 11bbbbba'aaaaaaaa'aaaaaaaa   offset a'aaaaaaaa'aaaaaaaa, length lookup_table[bbbbb]
            // EOF = 01x00000   end of file
            if (backrefSize == 1)
            {
                *dst++ = (uint8_t)(0x40 | offset | ((length-3)<<5));
            }
            else if (backrefSize == 2)
            {
                *dst++ = (uint8_t)(0x80 | ((length-3)<<3) | (offset >> 8));
                *dst++ = (uint8_t)(offset);
            }
            else if (backrefSize == 3)
            {
                *dst++ = (uint8_t)(0xC0 | (_FC8_LENGTH_ENCODE_LUT[length]<<1) | (offset >> 16));
                *dst++ = (uint8_t)(offset >> 8);
                *dst++ = (uint8_t)(offset);
            }

            /* Skip ahead (and update search accelerator)... */
            for (i = 1; i < length; ++i)
                UpdateLastPos(sa, in, src + i);
            src += length;
        }
        else
        {
            // literal
            if (literalRunLength == 0)
            {
                pRunLengthByte = dst;
                *dst++; // skip a byte for the run length
            }

            // output the literal
            if (dst >= outEnd) goto overflow;
            *dst++ = symbol;
            src++;
            literalRunLength++;

            // terminate the run if literal run length has reached max
            if (literalRunLength == _FC8_LONGEST_LITERAL_RUN)
            {
                *pRunLengthByte = literalRunLength-1;
                literalRunLength = 0;
            }
        }
    }

    if (literalRunLength)
    {
        // terminate the final literal run, if any
        *pRunLengthByte = literalRunLength-1;
    }

    // insert EOF
    *dst++ = 0x40;

    /* Free resources */
    SearchAccel_Destroy(sa);

    compressedSize = dst - out;

    /* Set header data */
    out[0] = 'F';
    out[1] = 'C';
    out[2] = '8';
    out[3] = '_';

    SetUInt32(out + FC8_DECODED_SIZE_OFFSET, insize);

    /* Return size of compressed buffer */
    return compressedSize;


overflow:
    /* Free resources */
    SearchAccel_Destroy(sa);

    /* Return size of compressed buffer */
    return 0;


fail:
    /* Exit routine for failure situations */
    if (sa)
        SearchAccel_Destroy(sa);
    return 0;
}


uint32_t Decode(const uint8_t *in, uint32_t insize, uint8_t *out, uint32_t outsize)
{
    uint8_t *src, *dst, symbol, symbolType;
    uint32_t  i, length, offset;

    /* Does the input buffer at least contain the header? */
    if (insize < FC8_HEADER_SIZE)
        return 0;

    /* Check magic number */
    if ((in[0] != 'F') || (in[1] != 'C') || (in[2] != '8') || (in[3] != '_'))
        return 0;

    /* Get & check output buffer size */
    if (outsize < GetUInt32(&in[FC8_DECODED_SIZE_OFFSET]))
        return 0;

    /* Initialize the byte streams */
    src = (unsigned char *)in;
    dst = out;
    
    /* Skip header information */
    src += FC8_HEADER_SIZE;
	
    /* Main decompression loop */
    while (1)
    {
        /* Get the next symbol */
        symbol = *src++;

        symbolType = symbol >> 6;

        switch (symbolType)
        {
        case 0:
            // LIT = 00aaaaaa  next aaaaaa+1 bytes are literals
            length = symbol+1;
            for (i=0; i<length; i++)
                *dst++ = *src++;
            break;

        case 1:
            // BR0 = 01baaaaa  backref offset aaaaa, length b+3
            length = 3 + ((symbol >> 5) & 0x01);
            offset = symbol & 0x1F;
            if (offset == 0)
                goto eof;
            for (i=0; i<length; i++)
                *dst++ = *(dst - offset);
            break;

        case 2:
            // BR1 = 10bbbaaa'aaaaaaaa   backref offset aaa'aaaaaaaa, length bbb+3
            length = 3 + ((symbol >> 3) & 0x07);
            offset = (((uint32_t)(symbol & 0x07)) << 8) | src[0];
            src++;
            for (i=0; i<length; i++)
                *dst++ = *(dst - offset);
            break;

        case 3:
            // BR2 = 11bbbbba'aaaaaaaa'aaaaaaaa   backref offset a'aaaaaaaa'aaaaaaaa, length lookup_table[bbbbb]
            length = _FC8_LENGTH_DECODE_LUT[(symbol >> 1) & 0x1f];
            offset = (((uint32_t)(symbol & 0x01)) << 16) | (((uint32_t)src[0]) << 8) | src[1];
            src += 2;
            for (i=0; i<length; i++)
                *dst++ = *(dst - offset);
            break;
        }
    }

eof:
    return dst - out;
}