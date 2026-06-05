# include "readtiff.h"

char const * path = "input/test.tif";

unsigned short endian_swap_2 (unsigned short in){
    short b0 = (in & 0x00ff) << 8, b1 = (in & 0xff00) >> 8; // Isolate and swap bytes.
    return b0 | b1; // Recombine bytes.
}

unsigned int endian_swap_4 (unsigned int in){
    short b0 = (in & 0x000000ff) << 24, // Isolate and swap bytes.
         b1 = (in & 0x0000ff00) << 8,
         b2 = (in & 0x00ff0000) >> 8,
         b3 = (in & 0xff000000) >> 24;
    return b0 | b1 | b2 | b3; // Recombine bytes.
}

int TIFF_read_keyval (FILE * fptr, TIFF_keyword keyword, int index, int swap){
    long start = ftell(fptr);

    unsigned short buf; fread(&buf, 2, 1, fptr); if (swap) buf = endian_swap_2(buf);
    int keycount = buf; // Number of keywords.

    for (int i = 0; i < keycount; i++) {
        fread(&buf, 2, 1, fptr); if (swap) endian_swap_2(buf); // Read keyword

        if (buf == keyword){ // Found keyword.
            fread(&buf, 2, 1, fptr); if (swap) buf = endian_swap_2(buf); // Read value type.
            if (buf > 4) return -1; // Value not an integer.
            int size = 8; if (buf > 2) size << (buf-2); // Get value size.

            int count; fread(&count, 4, 1, fptr); if (swap) count = endian_swap_4(count); // Read data count.
            int data; fread(&data, 4, 1, fptr); if (swap) data = endian_swap_4(data); // Read data/offset.
            if (count == 1){ // Data is not an array.
                fseek(fptr, start, SEEK_SET); // Go back to start.
                return data;
            }
            fseek(fptr, data, SEEK_SET); // Offset to data array.

            if (index >= count) return -1; // Index out of bounds.
            fseek(fptr, index * (size >> 8), SEEK_CUR); // Offet to correct index.
            fread(&data, 4, 1, fptr);

            fseek(fptr, start, SEEK_SET); // Go back to start.
            return data;
        }
    
        fseek(fptr, 10, SEEK_CUR); // Go to next keyword.
    }

    fseek(fptr, start, SEEK_SET); // Go back to start.
    return -1;
}

int main (){
    FILE * fptr = fopen(path, "rb");
    if (fptr == NULL) { printf("Bad path.\n"); return -1; }

    char buf [2]; if (fread(buf, 1, 2, fptr) != 2) { printf("Bad TIFF.\n"); return -1; }
    int little_endian;
    if (buf[0] == 'I' && buf[1] == 'I') little_endian = 1; // Little endian
    else if (buf[0] == 'M' && buf[1] == 'M') little_endian = 0; // Big endian
    else return -1; 

    unsigned short magic; if (fread(&magic, 2, 1, fptr) != 1) { printf("Bad TIFF.\n"); return -1; }
    if (!little_endian) magic = endian_swap_2(magic);
    if (magic != 42) { printf("Bad TIFF.\n"); return -1; }

    int IFD_adress; fread(&IFD_adress, 4, 1, fptr);
    fseek(fptr, IFD_adress, SEEK_SET);

    int width = TIFF_read_keyval(fptr, ImageWidth, 0, !little_endian),
    height = TIFF_read_keyval(fptr, ImageLength, 0, !little_endian),
    bitpix = TIFF_read_keyval(fptr, BitsPerSample, 0, !little_endian),
    offset = TIFF_read_keyval(fptr, StripOffsets, 0, !little_endian);
    fseek(fptr, offset, SEEK_SET);

    FILE * out = fopen("out.ppm", "wb");
    fprintf(out, "P6\n%d %d\n255\n", width, height);
    int rightshift = (bitpix >> 4)*8;
    for (int i = 0; i < width * height * 3; i++){
        unsigned short temp;
        fread(&temp, bitpix >> 3, 1, fptr);
        if (!little_endian) temp = endian_swap_2(temp);
        temp = temp >> rightshift;
        fputc(temp, out);
    }

    fclose(fptr); fclose(out);
    return 0;
}