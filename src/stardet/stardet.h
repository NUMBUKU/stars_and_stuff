# ifndef STARDET_H__
# define STARDET_H__

# include <stdio.h>

// Filetypes
typedef enum __ftypeenum {
    PGM, // .pgm
    FITS, // .fit(s)
    TIFF, // .tif(f)
    __inv = -1 // Invalid
} file_type;

// Image struct.
typedef struct __filestr {
    FILE * file; // File path.
    int width; // Image width.
    int height; // Image height.
    int max; // Image max value.
    int thres; // Star detection threshold.
    file_type type; // File type.
    int grey; // 0 file is RGB, 1 if file is greyscale.
    double avg; // Average pixel value.
    unsigned short * data; // Data array. To be indexed with the 'ind' function
} starfile;

// Vector struct.
typedef struct __vectstr {
    double x;
    double y;
} vect;

// Star struct.
typedef struct __starstr {
    vect pos; // Star position.
    double e; // Star eccentricity.
    double angle; // Star major axis inclination.
    double FWHM; // Star full width at half maximum.
    double SNR; // Star dignal to noise ratio.
    double HFD; // Star half flux diameter.
} star;

/// @brief Calculates the index of a row/collumn pair in the image array.
/// @note This is only the index of the gray value of the pixel. Add 1, 2 or 3 to the returned value to obtain indeces of the R, G and B data.
int ind (int row, int col, starfile * img);

/// @brief Reads the contents of a file into a given picture struct.
/// @param path Should have .pgm or .fits extension.
/// @return -1 if file invalid, -2 if allocation failed, 0 otherwise.
int read_starfile (char const * path, starfile * img);

/// @brief Extracts stars from the given file into the given array.
/// @param img Should be run through the "read_starfile" function first.
/// @param stars Should be the size of N_stars or bigger.
/// @param N_stars Maximum number of stars to extract.
/// @return Number of extracted stars.
int extract_stars (starfile * img, star stars [], int N_stars);

# endif
