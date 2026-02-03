# ifndef READFITS_H__
# define READFITS_H__

# include <stdio.h>
# include <math.h>

/// @brief Places the file pointer at the start of the data array.
/// @return -1 if the END keyword cannot be found or file ended preemptively, 0 otherwise.
int goto_end (FILE * fptr);

/// @brief Reads the value of the given keyword.
/// @return This value; NAN if the keyword cannot be found.
float read_keyval (FILE * fptr, char const keyword [9]);

/// @brief Extract bayer pattern from file.
/// @param bayer String to store bayer pattern.
/// @return -1 if BAYERPAT keyword cannot be found, 0 otherwise.
int read_bayer (FILE * fptr, char bayer [5]);

# endif
