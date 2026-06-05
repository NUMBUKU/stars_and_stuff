# ifndef READTIFF_H__
# define READTIFF_H__

# include <stdio.h>
# include <math.h>

typedef enum {
    ImageWidth = 256,
    ImageLength = 257,
    SamplesPerPixel = 277,
    BitsPerSample = 258,
    StripByteCounts = 279,
    StripOffsets = 273
} TIFF_keyword;

/// @brief Reads the value of the given keyword.
/// @param index If given value is stored in an array, the value at this index will be returned.
/// @param swap If 1 file will be read using endian swaps.
/// @return This value; NAN if the keyword cannot be found.
int TIFF_read_keyval (FILE * fptr, TIFF_keyword keyword, int index, int swap);



# endif
