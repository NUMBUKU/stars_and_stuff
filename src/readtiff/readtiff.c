# include <stdio.h>

char const * path = "input/test.tif";

unsigned short endian_swap (unsigned short in){
    short b0 = (in & 0xff) << 8, b1 = in >> 8; // Isolate bytes.
    return b0 | b1; // Swap bytes.
}

int main (){
    FILE * fptr = fopen(path, "rb");
    if (fptr == NULL) { printf("Bad path.\n"); return -1; }

    char buf [2]; if (fread(buf, 1, 2, fptr) != 2) { printf("Bad TIFF.\n"); return -1; }
    int big_endian;
    if (buf[0] == 'I' && buf[1] == 'I') big_endian = 1;
    else if (buf[0] == 'M' && buf[1] == 'M') big_endian = 0;
    else { printf("Bad TIFF.\n"); return -1; }

    unsigned short magic; if (fread(&magic, 2, 1, fptr) != 1) { printf("Bad TIFF.\n"); return -1; }
    if (!big_endian) magic = endian_swap(magic);
    if (magic != 42) { printf("Bad TIFF.\n"); return -1; }

    int IFD_adress; fread(&IFD_adress, 4, 1, fptr);
    fseek(fptr, IFD_adress, SEEK_SET);

    fclose(fptr);
    return 0;
}