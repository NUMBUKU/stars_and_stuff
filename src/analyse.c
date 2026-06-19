# include <stdio.h>
# include <stdlib.h>
# include <math.h>

# include "readfits/readfits.h"
# include "stardet/stardet.h"

# ifndef M_PI
# define M_PI 3.14159265358979323846
# endif

// Some constants.
int const N_stars = 1000, // Max number of stars to extract.
HIST_RES = 10; // Histogram x and y resolution.

starfile img;
double res = 0.0;

// Threshold above which a pixel will be checked for being a star.
int detection_threshold (){
    double stddev = 0.0;
    int imgsize  = img.width*img.height;

    // Calculate standard deviation
    for (int i = 0; i < imgsize; i++){ double dif = ( double ) img.data[i << 2] - img.avg; stddev += dif*dif; }
    stddev = sqrt(stddev / ( double ) imgsize);

    return ceil(img.avg + stddev * 4.0);
}


/// @brief Closes file and free arrays so program can end safely.
void close (){
    free(img.data); fclose(img.file);
}

/// @brief Round function.
/// @return An integer, useful for indexing.
int rd (double x){
    return round(x);
}

/// @brief Prints an error message and exits with the given code. (Exept if that code is zero)
void errhandle (int code){
    switch (code){
        case 1:
            printf("No path provided.\n");
            break;
        case -1:
            printf("File invalid.\n");
            break;
        case -2:
            printf("Allocation failed.\n");
            break;
        default:
            return;
    }

    exit(code);
}

/// @brief Prints the histogram and calculates the maximum and standard deviation, storing them at the provided adresses.
/// @return The calculated minimum.
int print_histogram (int * max, double * stddev){
    int count [HIST_RES],
    unit = img.max / (HIST_RES - 1),
    min = img.max; *max = 0; *stddev = 0;
    for (int i = 0; i < HIST_RES; i++) count[i] = 0;

    // Calculate min, max, stddev and collumn heights for histogram.
    for (int row = 0; row < img.height; row++) for (int col = 0; col < img.width; col++){
        if (img.data[ind(row, col, &img)] < min) min = img.data[ind(row, col, &img)];
        if (img.data[ind(row, col, &img)] > *max) *max = img.data[ind(row, col, &img)];
        double d = ( double ) img.data[ind(row, col, &img)] - img.avg;
        *stddev += d*d;
        count[img.data[ind(row, col, &img)] / unit]++;
    }
    *stddev = sqrt(*stddev / (img.width*img.height));

    // Print histogram.
    for (int y = HIST_RES; y > 0; y--){
        if (y == HIST_RES) printf("^  "); else printf("|  ");
        for (int x = 0; x < HIST_RES; x++){
            if (log10(count[x]) >= ( double ) y) printf("#  ");
        }
        printf("\n");
    }
    printf("   ");
    for (int x = 0; x < HIST_RES; x++) printf("---");
    printf(">\n");

    return min;
}

/// @brief Multiplies data by amp and clips the result to be smaller than or equal to max.
int mult (int data, int amp, int max){
    int multiplied = amp * data;
    return multiplied > max ? max : multiplied;
}

/// @brief Writes the file to a PPM file and marks the stars.
void mark_stars (star stars [], int N, int amp){
    FILE * out = fopen("marked.ppm", "wb");
    fprintf(out, "P6\n%d %d\n255\n", img.width, img.height);
    int rightshift = 8 * ((img.max + 1) >> 16); // Conversion factor from 16-bit to 8-bit.

    unsigned short * buf = ( unsigned short * ) malloc(img.height * img.width * 4 * sizeof(unsigned short)); // Allocate a buffer for debayered image.
    if (buf == NULL) errhandle(-2); // Malloc failed.
    if (img.grey) for (int i = 0; i < img.height * img.width * 4; i++) buf[i] = img.data[(i/4)*4];
    else for (int i = 0; i < img.height * img.width * 4; i++) buf[i] = img.data[i];

    for (int i = 0; i < N; i++){
        int x = rd(stars[i].pos.x), y = rd(stars[i].pos.y),
        centerpos = ind(y, x, &img);
        buf[centerpos+1] = img.max; buf[centerpos+2] = 0; buf[centerpos+3] = 0; // Mark star centre red

        // Encircle star green
        double s = sin(stars[i].angle * M_PI / 180.0), c = cos(stars[i].angle * M_PI / 180.0),
        asq = sqrt(pow(stars[i].FWHM, 4) / (1 - stars[i].e*stars[i].e)), bsq = sqrt(pow(stars[i].FWHM, 4) * (1 - stars[i].e*stars[i].e)),
        width = 1 / (stars[i].FWHM);
        int xlow = x > 2*stars[i].FWHM ? x - 2*stars[i].FWHM : 0,
        ylow = y > 2*stars[i].FWHM ? y - 2*stars[i].FWHM : 0;

        for (int row = ylow; row < y + 2*stars[i].FWHM && row < img.height; row++)
            for (int col = xlow; col < x + 2*stars[i].FWHM && col < img.width; col++){
                double xdist = ( double ) row - stars[i].pos.y, ydist = ( double ) col - stars[i].pos.x,
                X = c*xdist + s*ydist, Y = c*ydist - s*xdist;            
                if (fabs(X*X/asq + Y*Y/bsq - 1.0) < width){
                    buf[ind(row, col, &img)+1] = 0; buf[ind(row, col, &img)+2] = img.max; buf[ind(row, col, &img)+3] = 0;
                }
            }
    }

    for (int row = 0; row < img.height; row++) for (int col = 0; col < img.width; col++){
        for (int i = 0; i < 3; i++) fputc(mult(buf[ind(row, col, &img) + i+1], amp, img.max) >> rightshift, out);
    }

    free(buf);
    fclose(out);
}

/// @brief Calculates average star statistics.
void calc_avg (star * avg, star stars [], int N){
    for (int i = 0; i < N; i++){
        avg->e += stars[i].e;
        avg->angle += stars[i].angle;
        avg->FWHM += stars[i].FWHM;
        avg->HFD += stars[i].HFD;
        avg->SNR += stars[i].SNR;
    }
    if (N > 0) { avg->e /= N; avg->angle /= N; avg->FWHM /= N; avg->HFD /= N; avg->SNR /= N; }
}

/// @brief Prints the parameters of a given star.
void print_star (star * star, int i, int avg){
    if (!avg){
        printf("Star No. %d: \n", i+1);
        printf("\tPosition: (%d, %d)\n", rd(star->pos.x), rd(star->pos.y));
    }
    printf("\tEccentricity: %.2lf\n", star->e);
    printf("\tInclination: %.0f°\n", star->angle);
    printf("\tFWHM: %.2lf px  (%.2lf\")\n", star->FWHM, star->FWHM * res);
    printf("\tHFD: %.2lf px  (%.2lf\")\n", star->HFD, star->HFD * res);
    printf("\tSNR: %.2lf dB\n", star->SNR);
}

/// @brief Prints the user interface.
void UI (star stars [], int N){
    char c;
    while (1){
        printf("Please select an option.\n[F]ile info  [H]istogram  [S]tatistics  [L]ist stars  [M]ark stars  [Q]uit\n");
        scanf("%c", &c);

        switch (c){
            case 'f': case 'F':
                char * filetype;
                if (img.type == PGM){ char * tmp = "Pixel Gray Map (PGM)"; filetype = tmp; }
                else if (img.type == FITS){ char * tmp = "Flexible Image Transport System (FITS)"; filetype = tmp; }
                else if (img.type == TIFF){ char * tmp = "Tagged Image File Format (TIFF)"; filetype = tmp; }
                printf("\nFile information:\n\tFile type: %s\n\tDimensions: %d X %d\n\tMaximum pixel value: %d\n", filetype, img.width, img.height, img.max);
                if (!img.type) printf("\tResolution: %.2lf \"/px\n", res);
                break;
            case 'h': case 'H':
                printf("\nHistogram (logarithmic):\n");
                double stddev;
                int max,
                min = print_histogram(&max, &stddev);
                printf("\nMinimum:   %d (%.2lf %%)\nMaximum:   %d (%.2lf %%)\nMean:      %d (%.2lf %%)\nDeviation: %.2lf\n", min, 100.0 * ( double ) min / ( double ) img.max, max, 100.0 * ( double ) max / ( double ) img.max, rd(img.avg), 100.0 * img.avg / ( double ) img.max, stddev);
                break;
            case 's': case 'S':
                star avg = {0}; calc_avg(&avg, stars, N);
                if (N == 1) printf("\nExtracted 1 star.\n"); else printf("\nExtracted %d stars.\n", N);
                printf("Mean pixel value: %d (%.1lf %%)\nThreshold: %d (%.1lf %%)\nAverage star statistics:\n", rd(img.avg), 100.0 * img.avg / ( double ) img.max, img.thres, 100.0 * ( double ) img.thres / ( double ) img.max);
                print_star(&avg, 0, 1);
                break;
            case 'l': case 'L':
                if (N == 1) printf("\nExtracted 1 star.\n\n");
                else printf("\nExtracted %d stars.\n\n", N);
                for (int i = 0; i < N; i++) print_star(&stars[i], i, 0);
                break;
            case 'm': case 'M':
                int amp;
                printf("\nEnter an amplification: "); scanf("%d", &amp);
                printf("Converting...\n");
                mark_stars(stars, N, amp);
                printf("Done!\n");
                break;
            case 'q': case 'Q':
                printf("\nExiting...\n");
                return;
            default:
                printf("Invalid, please try again.\n");
        }
        printf("\n");
        scanf("%c", &c);
    }
}

/// @brief Calculates the resolution in arcsecs/px from the file metadata. (FITS only)
double get_resolution (){
    if (img.type != FITS) return res;

    double focal_length = FITS_read_keyval(img.file, "FOCALLEN");
    if (isnan(focal_length)) return res;

    double pixel_size = FITS_read_keyval(img.file, "XPIXSZ  ");
    if (isnan(pixel_size)) return res;

    return 206.265 * pixel_size / focal_length; // Credit: https://astronomy.tools/calculators/ccd
}

int main (int argc, char const ** argv){
    star stars [N_stars];
    int err;

    err = argc < 2; errhandle(err); // Check for path

    err = read_starfile(argv[1], &img); errhandle(err); // Read file
    res = get_resolution();
    img.thres = detection_threshold();

    int n_extracted_stars = extract_stars(&img, stars, N_stars); // Extract star positions

    UI(stars, n_extracted_stars); // Print user interface

    close();
    return 0;
}