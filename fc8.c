/*
* FC8 compression by Steve Chamberlin
* Derived from liblzg by Marcus Geelnard
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
    int arg;

    // Default arguments
    inName = NULL;
    outName = NULL;

    // Get arguments
    for (arg = 1; arg < argc; ++arg)
    {
        if (strcmp("-d", argv[arg]) == 0)
            decompress = 1;
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
        maxOutSize = GetDecodedSize(inBuf);
    }
    else
    {
        // Guess maximum size of compressed data in worst case
        maxOutSize = inSize * 2; 
    }

    // Allocate memory for the output data
    outBuf = (unsigned char*) malloc(maxOutSize);
    if (outBuf)
    {
        // Compress/Decompress
        if (decompress)
            outSize = Decode(inBuf, inSize, outBuf, maxOutSize);
        else
            outSize = Encode(inBuf, inSize, outBuf, maxOutSize);

        if (outSize)
        {
            if (decompress)
                fprintf(stderr, "Decompressed file is %d byte\n", outSize);
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
