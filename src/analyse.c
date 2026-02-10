# include <stdio.h>

# include "readfits/readfits.h"
# include "stardet/stardet.h"

// Some constants.
int const N_stars = 300, // Max number of stars to extract.
HIST_RES = 10; // Histogram x and y resolution.

// Threshold above which a pixel will be checked for being a star.
int detection_threshold (double avg){
    return ( int ) ceil(avg * 3.0);
}


picture img;
double res = 0.0;

/// @brief Closes file and free arrays so program can end safely.
void close (){
    free(img.data); fclose(img.file);
}

/// @brief Round function.
/// @return An integer, useful for indexing.
int rd (double x){
    return ( int ) round(x);
}

/// @brief Prints an error message and exits with the given code. (Exept if that code is zero)
void errhandle (int code){
    switch (code){
        case 1:
            printf("No path provided.\n");
            break;
        case -1:
            printf("Couldn't open file at provided path.\n");
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

/// @brief Prints the histogram and calculates the maximum and standard deviation, storing them at the provided adresses.
/// @return The calculated minimum.
int print_histogram (int * max, double * stddev){
    int count [HIST_RES],
    unit = img.max / (HIST_RES - 1),
    min = img.max; *max = 0; *stddev = 0;
    for (int i = 0; i < HIST_RES; i++) count[i] = 0;

    // Calculate min, max, stddev and collumn heights for histogram.
    for (int row = 0; row < img.height; row++) for (int col = 0; col < img.width; col++){
        if (img.data[row*img.width*4 + col*4] < min) min = img.data[row*img.width*4 + col*4];
        if (img.data[row*img.width*4 + col*4] > *max) *max = img.data[row*img.width*4 + col*4];
        double d = ( double ) img.data[row*img.width*4 + col*4] - img.avg;
        *stddev += d*d;
        count[img.data[row*img.width*4 + col*4] / unit]++;
    }
    *stddev = sqrt(*stddev / (img.width*img.height));

    // Print histogram.
    for (int y = HIST_RES; y > 0; y--){
        if (y == HIST_RES) printf("^  "); else printf("|  ");
        for (int x = 0; x < HIST_RES; x++){
            if (log10(( double ) count[x]) >= ( double ) y) printf("#  ");
        }
        printf("\n");
    }
    printf("   ");
    for (int x = 0; x < HIST_RES; x++) printf("---");
    printf(">\n");

    return min;
}

/// @brief Writes the file to a PPM file and marks the stars.
void mark_stars (star stars [], int N){
    FILE * out = fopen("marked.ppm", "wb");
    fprintf(out, "P6\n%d %d\n255\n", img.width, img.height);
    for (int row = 0; row < img.height; row++) for (int col = 0; col < img.width; col++){
        int close = 0, center = 0;
        for (int i = 0; i < N; i++){
            if (rd(stars[i].pos.y) == row && rd(stars[i].pos.x) == col) { center = 1; break; }
            int xdist = row - stars[i].pos.y, ydist = col - stars[i].pos.x;
            if (fabs(xdist*xdist + ydist*ydist - stars[i].FWHM*stars[i].FWHM) < stars[i].FWHM) { close = 1; break; }
        }

        if (close) { fputc(0, out); fputc(255, out); fputc(0, out); } // Green.
        else if (center) { fputc(255, out); fputc(0, out); fputc(0, out); } // Red.
        else for (int i = 0; i < 3; i++) fputc(img.data[row*img.width*4 + col*4 + i + 1] >> 8, out); // Data. (Greyscale)
    }
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
                printf("\nFile information:\n\tFile type: %s\n\tDimensions: %d X %d\n\tMaximum pixel value: %d\n", img.PGM ? "Pixel Gray Map (PGM)" : "Flexible Image Transport System (FITS)", img.width, img.height, img.max);
                if (!img.PGM) printf("\tResolution: %.2lf \"/px\n", res);
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
                printf("\nConverting...\n");
                mark_stars(stars, N);
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

/// @brief Calculates the resolution from the file metadata. (FITS only)
float get_resolution (){
    if (img.PGM) return res;

    double focal_length = read_keyval(img.file, "FOCALLEN");
    if (isnan(focal_length)) return res;

    double pixsc = read_keyval(img.file, "XPIXSZ  ");
    if (isnan(pixsc)) return res;

    return 206.265 * pixsc / focal_length; // Credit: https://astronomy.tools/calculators/ccd
}

int main (int argc, char const ** argv){
    star stars [N_stars]; int err;

    err = argc < 2; errhandle(err); // Check for path

    err = read_starfile(argv[1], &img); errhandle(err); // Read file
    res = get_resolution();
    img.thres = detection_threshold(img.avg);

    int n_extracted_stars = extract_stars(&img, stars, N_stars); // Extract star positions

    UI(stars, n_extracted_stars); // Print user interface

    close();
    return 0;
}