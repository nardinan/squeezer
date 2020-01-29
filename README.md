## Squeeeeeeeeeze me

I was on a flight for Copenhagen and I got bored, so I wrote a small idiotic application to compress ASCII files. It is based on an idea I had when I was a kid (16-17 years ago) and I was implementing my first network text-based protocol for a small game I never released. The idea (cool for a kid, not really for an adult) is to parse the file creating a dictionary that could be used to encode a pair of bytes into a single byte, using the unused part of that byte. 

To clarify, in the ASCII table each character is mapped to a number from 0 to 127 (128 entries) and, this means, that an unsigned char has an extra bit on its most significant part that can be used to map other 128 elements in the very same space.

The system works in a super simple way (and I am pretty sure there are much better ways to do it):

1 - It scans the content of the file creating a dictionary: a pair of bytes associated with its occurrences. It keeps the map sorted from the pair with the highest number of occurrences to the one with the lowest.

2 - The map is then truncated to take into consideration only the first 128 entries.

3 - The table is written in the output file as header (2byte * 128 entries), then the input file is re-read and written in the output file at the very same time. Each pair of bytes found in the input file and present in the compression table are converted into (127 + index of the couple in the map).

During the decompression a similar process is done:

1 - The first (2bytes * 128 entries) are read and mapped.

2 - For each byte read, if its value is above 127 then it is converted into a couple of bytes accordingly with its position in the map and stored into the final file.

Pretty easy, uh?

## OK but, why?

I was bored and tired so my mind flew away, and I started to think about the game I made when I was 15 and its stooping text-based protocol. A little bit of nostalgia and ... tack! Is pretty easy and I am pretty sure there are plenty of implementations (better implementations) all around.

## How to use it?

Well, it is a single .c file so, you can use your amazing gcc (or clang) compiler to compile it:

```
gcc -o squeezer squeezer.c
```

Then you can use it in two ways:

```
./squeezer enc input_file.txt compressed_file.squeezed
```

This compresses the file input_file.txt into compressed_file.squeezed

```
./squeezer dec compressed_file.squeezed decompressed_file.txt
```

This decompresses the file compressed_file.squeezed into decompressed_file.txt
