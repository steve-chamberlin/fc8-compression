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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "fc8.h"

#ifdef _WIN32
  #include <io.h>
  #include <fcntl.h>
#endif

void ShowUsage(char *prgName)
{
    fprintf(stderr, "Usage: %s [options] infile [outfile]\n", prgName);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, " -b:NNN  compress the input as multiple independent blocks of size NNN bytes\n");
    fprintf(stderr, " -d  decompress\n");
    fprintf(stderr, "\nIf no output file is given, stdout is used for output.\n");
}

int main(int argc, char **argv)
{
    char *inName, *outName;
    FILE *inFile, *outFile;
    uint32_t inSize = 0, outSize = 0;
    uint32_t maxOutSize;
    uint8_t *inBuf, *outBuf;
    size_t fileSize;
    uint8_t decompress = 0;
    uint32_t blockSize = 0;
    uint32_t i, numBlocks = 0;
    int arg;

    // Default arguments
    inName = NULL;
    outName = NULL;

    // Get arguments
    for (arg = 1; arg < argc; ++arg)
    {
        if (strcmp("-d", argv[arg]) == 0)
            decompress = 1;
        else if (strncmp("-b", argv[arg], 2) == 0)
        {
            if (argv[arg][2] != ':')
                ShowUsage(argv[0]);
            blockSize = atoi(&argv[arg][3]);
        }
        else if (!inName)
            inName = argv[arg];
        else if (!outName)
            outName = argv[arg];
        else
        {
            ShowUsage(argv[0]);
            return 0;
        }
    }
    if (!inName)
    {
        ShowUsage(argv[0]);
        return 0;
    }

    if (decompress && blockSize != 0)
    {
        fprintf(stderr, "Block size will be read from the input data, -b option ignored\n");
    }

    // Read input file
    inBuf = (unsigned char*) 0;
    inFile = fopen(inName, "rb");
    if (inFile)
    {
        fseek(inFile, 0, SEEK_END);
        fileSize = (size_t) ftell(inFile);
        fseek(inFile, 0, SEEK_SET);
        if (fileSize > 0)
        {
            inSize = (uint32_t) fileSize;
            inBuf = (unsigned char*) malloc(inSize);
            if (inBuf)
            {
                if (fread(inBuf, 1, inSize, inFile) != inSize)
                {
                    fprintf(stderr, "Error reading \"%s\".\n", inName);
                    free(inBuf);
                    outBuf = (unsigned char*) 0;
                }				
            }
            else
                fprintf(stderr, "Out of memory.\n");
        }
        else
            fprintf(stderr, "Input file is empty.\n");

        fclose(inFile);
    }
    else
        fprintf(stderr, "Unable to open file \"%s\".\n", inName);

    if (!inBuf)
        return 0;

    if (decompress)
    {
        // determine blockSize and numBlocks
        if (inBuf[0] != 'F' || inBuf[1] != 'C' || inBuf[2] != '8' || (inBuf[3] != '_' && inBuf[3] != 'b'))
        {
            fprintf(stderr, "Input is not an FC8 compressed file.\n");
            return 0;
        }
        
        maxOutSize = GetUInt32(&inBuf[FC8_DECODED_SIZE_OFFSET]);

        if (inBuf[3] == 'b')
        {
            blockSize = GetUInt32(&inBuf[FC8_BLOCK_SIZE_OFFSET]);
            numBlocks = (maxOutSize + blockSize - 1) / blockSize;
            fprintf(stderr, "Decompressing block format with %d byte blocks\n", blockSize);
        }
        else
        {
            blockSize = maxOutSize;
            numBlocks = 1;
        }   
    }
    else
    {
        // Estimate the maximum size of compressed data in worst case
        maxOutSize = inSize * 2; 

        if (blockSize != 0)
        {
            numBlocks = (inSize + blockSize - 1) / blockSize;
        }
        else
        {
            blockSize = inSize;
            numBlocks = 1;
        }
    }

    // Allocate memory for the output data
    outBuf = (unsigned char*) malloc(maxOutSize);
    if (outBuf)
    {
        // If compressing block format, add the block header
        if (!decompress && blockSize != inSize)
        {
            // Set header data
            outBuf[0] = 'F';
            outBuf[1] = 'C';
            outBuf[2] = '8';
            outBuf[3] = 'b';

            // Uncompressed file size
            SetUInt32(outBuf + FC8_DECODED_SIZE_OFFSET, inSize);

            // block size
            SetUInt32(outBuf + FC8_BLOCK_SIZE_OFFSET, blockSize);

            // skip block header
            outSize += FC8_BLOCK_HEADER_SIZE;

            // skip block offsets
            outSize += numBlocks * sizeof(uint32_t);
        }

        // process all the blocks
        for (i=0; i<numBlocks; i++)
        {
            if (decompress)
            {
                uint32_t blockOffset = 0, processedBlockSize;

                // decompressing block format?
                if (blockSize != maxOutSize)
                    blockOffset = GetUInt32(&inBuf[FC8_BLOCK_HEADER_SIZE + sizeof(uint32_t) * i]);

                processedBlockSize = Decode(inBuf + blockOffset, FC8_HEADER_SIZE, outBuf + blockSize * i, maxOutSize - blockSize * i);

                // error?
                if (processedBlockSize != blockSize)
                {
                    outSize = 0;
                    break;
                }

                outSize += processedBlockSize;
            }
            else
            {
                uint32_t processedBlockSize = Encode(inBuf + blockSize * i, blockSize, outBuf + outSize, maxOutSize - outSize);
                
                // error?
                if (!processedBlockSize)
                {
                    outSize = 0;
                    break;
                }

                // compressing block format?
                if (blockSize != inSize)
                {
                    // update the block offset in the block header
                    SetUInt32(&outBuf[FC8_BLOCK_HEADER_SIZE + sizeof(uint32_t) * i], outSize);
                }

                outSize += processedBlockSize;
            }
        }

        // save result
        if (outSize)
        {
            if (decompress)
                fprintf(stderr, "Decompressed file is %d bytes\n", outSize);
            else
                fprintf(stderr, "Result: %d bytes (%d%% of the original)\n", outSize, (100 * outSize) / inSize);

            // Processed data is now in outBuf, write it...
            if (outName)
            {
                outFile = fopen(outName, "wb");
                if (!outFile)
                    fprintf(stderr, "Unable to open file \"%s\".\n", outName);
            }
            else
            {
                #ifdef _WIN32
			_setmode(_fileno(stdout),O_BINARY);
		  #endif
                outFile = stdout;
            }

            if (outFile)
            {
                // Write data
                if (fwrite(outBuf, 1, outSize, outFile) != outSize)
                    fprintf(stderr, "Error writing to output file.\n");

                // Close file
                if (outName)
                    fclose(outFile);
            }
        }
        else
            fprintf(stderr, "Operation failed!\n");

        // Free memory when we're done with the compressed data
        free(outBuf);
    }
    else
        fprintf(stderr, "Out of memory!\n");

    // Free memory
    free(inBuf);

    return 0;
}
