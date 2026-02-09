# include <stdio.h>
# include <dirent.h>

# include "readfits/readfits.h"
# include "stardet/stardet.h"

int width = 0, height = 0; // Width and height of first image

void closeimg (picture * img){
    for (int row = 0; row < img->height; row++) free(img->data[row]);
    free(img->data); fclose(img->file);
}

void errhandle (int code){
    switch (code){
        case 1:
            printf("No path provided.\n");
            break;
        case -1:
            printf("Couldn't open directory at provided path.\n");
            break;
        case -2:
            printf("File extension invalid.\n");
            break;
        case -3:
            printf("Allocation failed.\n");
            break;
        case -4:
            printf("Invalid file.\n");
            break;
        default:
            return;
    }

    exit(code);
}

int const full_path_len (char const * dir, char const * file){
    int i = 0;
    while (dir[i] != '\0'){
        i++;
    }

    int j = 0;
    while (file[j] != '\0'){
        j++;
    }

    int const len = i + j + 2;
    return len;
}

void get_full_path (char * path, char const * dir, char const * file){
    int i = 0;
    while (dir[i] != '\0'){
        i++;
    }

    int j = 0;
    while (file[j] != '\0'){
        j++;
    }

    int const len = i + j + 2;

    for (int k = 0; k < len; k++){
        if (k < i) path[k] = dir[k];
        else if (k == i) path[k] = '/';
        else path[k] = file[k - i - 1];
    }
}

int check_FITS (char const * path){
    char c = ' '; int i, pos = -1;

    // Find the last . in file extension
    for (i = 0; c != '\0'; i++){
        c = path[i];
        if (c == '.') pos = i;
    }

    if (pos == -1) return 0; // Could not find a .

    i = pos+1;
    if (path[i] == 'f' && path[i+1] == 'i' && path[i+2] == 't' && path[i+3] == 's' && path[i+4] == '\0') return 1; // FITS file
    
    return 0; // Other file type
}

void add_filedata (picture * img, double * data, int n){
    if (n == 0){
        width = img->width; height = img->height;
        for (int row = 0; row < img->height; row++) for (int col = 0; col < img->width; col++){
            data[row*img->width + col] = ( double ) img->data[row][col];
        }
        return;
    }

    for (int row = 0; row < height; row++) for (int col = 0; col < width; col++){
        data[row*width + col] += ( double ) img->data[row][col];
    }

    return;
}

void readwrite (DIR * directory, char const * dirname){
    int n = 0; // Counter
    double * data; // Array for storing all the file data
    struct dirent * file = readdir(directory);

    // read
    while (file != NULL){
        char const * filename = file->d_name;
        int const len = full_path_len(dirname, filename);
        char path [len]; get_full_path(path, dirname, filename);

        if (check_FITS(path)){ // Only read FITS files
            picture img;
            errhandle(read_starfile(path, &img));
            if (n != 0 && (width != img.width || height != img.height)) continue; // Image not the correct size
            printf("Found a FITS file! Name: %s\n", filename);

            if (n == 0){
                data = ( double * ) malloc(img.width * img.height * sizeof(double));
                if (data == NULL) errhandle(-3);
            }

            add_filedata(&img, data, n);
            n++;

            closeimg(&img);
        }

        file = readdir(directory); // Get the next file in the directory
    }

    // write
    FILE * out = fopen("avg.pgm", "wb");
    fprintf(out, "P5\n%d %d\n255\n", width, height);
    for (int row = 0; row < height; row++) for (int col = 0; col < width; col++){
        data[row*width + col] /= ( double ) n;
        fputc(( int ) data[row*width + col] >> 8, out);
    }

    if (n != 0) free(data); fclose(out);
}

int main (int argc, char ** argv){
    errhandle(argc < 2);

    DIR * directory = opendir(argv[1]);
    if (directory == NULL) errhandle(-1);
    printf("Given directory: %s\n", argv[1]);

    readwrite(directory, argv[1]);

    closedir(directory);
    return 0;
}
