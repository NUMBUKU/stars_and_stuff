# include "readtiff.h"

# include <math.h>

int TIFF_read_keyval (FILE * fptr, TIFF_keyword keyword, int index){
    long start = ftell(fptr);

    unsigned short buf; fread(&buf, 2, 1, fptr);
    int keycount = buf; // Number of keywords.

    for (int i = 0; i < keycount; i++) {
        fread(&buf, 2, 1, fptr);// Read keyword

        if (buf == keyword){ // Found keyword.
            fread(&buf, 2, 1, fptr); // Read value type.
            if (buf > 4){ fseek(fptr, start, SEEK_SET); return -1; } // Value not an integer.
            int size = 1; if (buf > 2) size += buf-2; // Get value size (bytes).

            int count; fread(&count, 4, 1, fptr); // Read data count.
            int data; fread(&data, 4, 1, fptr); // Read data/offset.
            if (count == 1){ // Data is not an array.
                fseek(fptr, start, SEEK_SET); // Go back to start.
                return data;
            }
            fseek(fptr, data, SEEK_SET); // Offset to data array.

            if (index >= count){ fseek(fptr, start, SEEK_SET); return -1; } // Index out of bounds.
            fseek(fptr, index * size, SEEK_CUR); // Offet to correct index.
            fread(&data, size, 1, fptr);

            fseek(fptr, start, SEEK_SET); // Go back to start.
            return data;
        }
    
        fseek(fptr, 10, SEEK_CUR); // Go to next keyword.
    }

    fseek(fptr, start, SEEK_SET); // Go back to start.
    return -1;
}

int TIFF_goto_end (FILE * fptr){
    int offset = TIFF_read_keyval(fptr, StripOffsets, 0);
    if (offset == -1) return -1;

    fseek(fptr, offset, SEEK_SET);
    return 0;
}
