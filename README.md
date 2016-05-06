# fc8-compression
FC8 is designed to be as fast as possible to decompress on "legacy" hardware, while still maintaining a decent compression ratio. Generic C code for compression and decompression is provided, as well as an optimized 68K decompressor for the 68020 or later CPUs. The main loop of the 68K decompressor is exactly 256 bytes, so it fits entirely within the instruction cache of the 68020/030. Decompression speed on a 68030 is about 25% as fast as an optimized memcpy of uncompressed data.

The algorithm is based on the classic LZ77 compression scheme, with a sliding history window and duplicated data replaced by (distance,length) markers pointing to previous instances of the same data. No extra RAM is required during decompression, aside from the input and output buffers. The implementation is derived from LZG by Marcus Geelnard, with much of the compression code borrowed from LZG, but the encoding and decompressor are new. Compared with LZG on a 68030 CPU, FC8 compresses data equally as tightly and decompresses 1.5x to 2x faster.

The compressed data is a series of tokens in this format:

LIT = 00aaaaaa  next aaaaaa+1 bytes are literals
BR0 = 01baaaaa  backref to offset aaaaa, length b+3
EOF = 01x00000  end of file
BR1 = 10bbbaaa'aaaaaaaa   backref to offset aaa'aaaaaaaa, length bbb+3
BR2 = 11bbbbba'aaaaaaaa'aaaaaaaa   backref to offset a'aaaaaaaa'aaaaaaaa, length lookup_table[bbbbb]

The length lookup table enables encoding of backrefs up to 256 bytes in length using only 5 bits, though some longer lengths can't be encoded directly. These are encoded as two successive backrefs, each with a smaller length.
