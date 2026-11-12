# Huffman Coding in C

- This is my implementation for huffman encoding and decoding in C.
- Huffman coding is entropy coding algorithm.
- This algorithm does NOT do any redundancy elimination (like LZSS) it's purpose is to shorten a data calculating frequency and giving them code depending on frequency.
- The stages for encoding:
  1. Calculate frequencies
  2. Build tree out of frequencies. (each node has sum of children's frequencies and each leaf has its own character and frequency count) (start creating nodes from least frequent one to most frequent one)
  3. Generate codes of each symbol. (leaf) Most frequent ones get shorter codes. (because we put them close to the top) Codes are in binary format with arbitrary length.
  4. Serialize the tree using codes and input (for order and repeats).
- Symbols are represented as codes in binary tree.
- Each symbol has its character and frequency count.

# Quick Start

```bash
make && ./main
```

- This will create output.bin here and it contains encoded data of "lossless".
- You can use `huffman_encode` to get encoded data from an input and use `write_encoded_data_to_file` function to generate special binary file for the decoder.
- This file format is described in that function's comment.
- (We don't have decoder right now)
- At top of the file, we have `DEBUG_PRINT` macro. Set it 1 to make encoder print what it generated at every step.

# References

- https://www.w3schools.com/dsa/dsa_ref_huffman_coding.php
- https://github.com/ilpenSE/str.h
- https://github.com/ilpenSE/bw.h
