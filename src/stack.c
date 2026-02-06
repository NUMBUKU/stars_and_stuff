# include <stdio.h>
# include <dirent.h>

# include "readfits/readfits.h"
# include "stardet/stardet.h"

int width = 0, height = 0;

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
    char c = ' '; int i;

    for (i = 0; c != '.'; i++){
        c = path[i];
        if (c == '\0') return 0; // Other file type
    }

    if (path[i] == 'f' && path[i+1] == 'i' && path[i+2] == 't' && path[i+3] == 's' && path[i+4] == '\0') return 1; // FITS file
    
    return 0; // Other file type
}

double * add_filedata (char const * path, double * data, int n){
    picture img;

    errhandle(read_starfile(path, &img));

    if (n == 0){
        double * ret = ( double * ) malloc(img.width * img.height * sizeof(double));
        if (ret == NULL) errhandle(-3);
        width = img.width; height = img.height;
        for (int row = 0; row < img.height; row++) for (int col = 0; col < img.width; col++){
            ret[row*img.width + col] = ( double ) img.data[row][col];
        }
        return ret;
    }

    if (width != img.width || height != img.height) return data;

    for (int row = 0; row < height; row++) for (int col = 0; col < width; col++){
        data[row*width + col] += ( double ) img.data[row][col];
    }
    return data;
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
            data = add_filedata(( char const * ) path, data, n);
            n++;
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

    free(data); fclose(out);
}

int main (int argc, char ** argv){
    errhandle(argc < 2);

    DIR * directory = opendir(argv[1]);
    if (directory == NULL) errhandle(-1);

    readwrite(directory, argv[1]);

    closedir(directory);
    return 0;
}
