# ifndef READTIFF_H__
# define READTIFF_H__

# include <stdio.h>

typedef enum _keyw {
    ImageWidth = 256,
    ImageLength = 257,
    SamplesPerPixel = 277,
    BitsPerSample = 258,
    StripByteCounts = 279,
    StripOffsets = 273
} TIFF_keyword;

/// @brief Reads the value of the given keyword.
/// @param index If given value is stored in an array, the value at this index will be returned.
/// @return This value; -1 if the keyword cannot be found.
int TIFF_read_keyval (FILE * fptr, TIFF_keyword keyword, int index);

/// @brief Places the file pointer at the start of the data array.
/// @return -1 if the StripOffsets keyword cannot be found or file ended preemptively, 0 otherwise.
int TIFF_goto_end (FILE * fptr);

# endif
