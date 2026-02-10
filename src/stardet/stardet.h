# ifndef STARDET_H__
# define STARDET_H__

# include <stdio.h>
# include <stdlib.h>
# include <math.h>

# include "../readfits/readfits.h"

// Image struct.
typedef struct {
    FILE * file; // File path.
    int width; // Image width.
    int height; // Image height.
    int max; // Image max value.
    int thres; // Star detection threshold.
    int PGM; // 0 if file is .fits type, 1 if file is .pgm file.
    double avg; // Average pixel value.
    unsigned short * data; // Data array.
} picture;

// Vector struct.
typedef struct {
    double x;
    double y;
} vect;

// Star struct.
typedef struct {
    vect pos; // Star position.
    double e; // Star eccentricity.
    double angle; // Star major axis inclination.
    double FWHM; // Star full width at half maximum.
    double SNR; // Star dignal to noise ratio.
    double HFD; // Star half flux diameter.
} star;

/// @brief Reads the contents of a file into a given picture struct.
/// @param path Should have .pgm or .fits extension.
/// @param RGB_to_mono RGB to monochrome conversion function.
/// @return -1 if path invalid, -2 if file invalid, -3 if allocation failed, 0 otherwise.
int read_starfile (char const * path, picture * img);

/// @brief Extracts stars from the given file into the given array.
/// @param img Should be run through the "read" function first.
/// @param stars Should be the size of N_stars or bigger.
/// @param N_stars Maximum number of stars to extract.
/// @return Number of extracted stars.
int extract_stars (picture * img, star stars [], int N_stars);

# endif
