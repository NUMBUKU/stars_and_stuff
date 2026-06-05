# include "stardet.h"

# ifndef M_PI
# define M_PI 3.14159265358979323846
# endif

// Some constants.
int const __FIND_MIDDLE_ITER = 3, // Number of iterations to find star center.
__MAX_STARSIZE = 40, // Maximum star size.
__RAYS = 10; // Number of rays to find major/minor axis. (should ALWAYS be even so there can be a minor/major pair)
double const __STAR_MARGIN = 2.0; // Minimum number of FWHM diameters between stars

/// @brief RGB to monochrome conversion function. (CIE 1931)
int __RGB_to_mono (int R, int G, int B){
    return ( int ) (0.2126 * ( double ) R + 0.7152 * ( double ) G + 0.0722 * ( double ) B);
}

/// @brief Calculates the index of a row/collumn pair in the image array.
int __ind (int row, int col, starfile * img){
    return (row*img->width + col) << 2;
}

/// @brief Perform a byte swap on the input.
/// @param in Should be either 1 or 2 bytes.
/// @return The byte swapped input.
signed short __endian_swap (unsigned short in){
    short b0 = (in & 0xff) << 8, b1 = in >> 8; // Isolate bytes.
    return b0 | b1; // Swap bytes.
}

/// @brief Close and free if an error ocurred.
/// @param img Picture to close/free from.
/// @param row Free data array up to this row.
/// @param arr Free img->data y/n.
void __close (starfile * img, int arr){
    fclose(img->file);
    if (arr) free(img->data);
}

/// @brief Round function.
/// @return An integer, useful for indexing.
int __rd (double x){
    return ( int ) round(x);
}

/// @brief Detects the file type of the given path.
/// @return 1 if PGM file; 0 if FITS file; -1 otherwise.
int __check_file_type (char const * path){
    char c = ' '; int i, pos = -1;

    // Find the last . in file extension
    for (i = 0; c != '\0'; i++){
        c = path[i];
        if (c == '.') pos = i;
    }

    if (pos == -1) return 0; // Could not find a .

    i = pos+1;
    if (path[i] == 'p' && path[i+1] == 'g' && path[i+2] == 'm' && path[i+3] == '\0') return PGM; // PGM file.
    else if (path[i] == 'f' && path[i+1] == 'i' && path[i+2] == 't' && path[i+3] == 's' && path[i+4] == '\0') return FITS; // FITS file.
    else if (path[i] == 'f' && path[i+1] == 'i' && path[i+2] == 't' && path[i+4] == '\0') return FITS; // FIT file.
    else if (path[i] == 't' && path[i+1] == 'i' && path[i+2] == 'f' && path[i+3] == '\0') return TIFF; // TIF file.
    else if (path[i] == 't' && path[i+1] == 'i' && path[i+2] == 'f' && path[i+3] == 'f' && path[i+4] == '\0') return TIFF; // TIFF file.
    
    return -1; // Not PGM or FITS: invalid file type.
}

/// @brief Reads the width, height and maximum pixel value.
/// @return -1 if file is invalid, 0 otherwise.
int __read_metadata (starfile * img){
    if (img->type == PGM){
        // Check PGM for validity.
        if (fgetc(img->file) != 'P' || fgetc(img->file) != '5') return -1; fgetc(img->file);

        // Read metadata.
        fscanf(img->file, "%d %d", &img->width, &img->height);
        fscanf(img->file, "%d", &img->max);
        img->grey = 1;

        return 0;
    } if (img->type == FITS){
        // Check FITS for validity.
        if (FITS_read_keyval(img->file, "SIMPLE  ") != ( float ) 'T') return -1;
        if (FITS_goto_end(img->file) == -1) return -1;

        // Read metadata.
        img->width = ( int ) FITS_read_keyval(img->file, "NAXIS1  ");
        img->height = ( int ) FITS_read_keyval(img->file, "NAXIS2  ");
        img->max = (1 << ( int ) FITS_read_keyval(img->file, "BITPIX  ")) - 1;

        return 0;
    } if (img->type == TIFF) {
        // Check TIFF for validity
        char buf [2]; if (fread(buf, 1, 2, img->file) != 2) return -1; // Read endianess
        int little_endian;
        if (buf[0] == 'I' && buf[1] == 'I') little_endian = 1; // Little endian
        else if (buf[0] == 'M' && buf[1] == 'M') little_endian = 0; // Big endian
        else return -1; 
        unsigned short magic; if (fread(&magic, 2, 1, img->file) != 1) return -1;
        if (!little_endian) magic = __endian_swap(magic);
        if (magic != 42) return -1;
    }

    return -1;
}

/// @brief Converts raw FITS data to physical data and debayers the FITS file if it has a Bayer pattern.
/// @return -1 if malloc failed, 0 otherwise.
int __debayer (starfile * img){
    int bias = ( int ) FITS_read_keyval(img->file, "BZERO   ");
    img->grey = 0;
    char bayer [5]; if (FITS_read_bayer(img->file, bayer) == -1) img->grey = 1; // Monochrome file handling.
    img->avg = 0.0; // Recalculate average.

    for (int row = 0; row < img->height; row++) for (int col = 0; col < img->width; col++){
        img->data[__ind(row, col, img)] = bias + __endian_swap(img->data[__ind(row, col, img)]); // Convert raw data to physical value.
        img->avg += img->data[__ind(row, col, img)];
    }
    img->avg /= ( double ) (img->width * img->height);
    if (img->grey) return 0;
    img->avg = 0.0; // Recalculate average again.

    // Debayering.
    unsigned short * buf = ( unsigned short * ) malloc(img->height * img->width * 4 * sizeof(unsigned short)); // Allocate a buffer for debayered image.
    if (buf == NULL) return -1; // Malloc failed.

    for (int row = 0; row < img->height; row++){
        for (int col = 0; col < img->width; col++){
            // Count the amount of colours neighbouring each pixel and add them up.
            int colour_count [] = {0, 0, 0}, colours [] = {0, 0, 0};
            for (int currow = row-1; currow < row+2; currow++) for (int curcol = col-1; curcol < col+2; curcol++){
                if (currow >= img->height || currow < 0 || curcol >= img->width || curcol < 0) continue;
                int bayer_index = (curcol % 2) + 2*(currow % 2),
                colour_index = 1;

                if (bayer[bayer_index] == 'R') colour_index = 0;
                else if (bayer[bayer_index] == 'G') colour_index = 1;
                else if (bayer[bayer_index] == 'B') colour_index = 2; 
                colour_count[colour_index]++;
                colours[colour_index] += img->data[__ind(currow, curcol, img)];
            }

            for (int i = 0; i < 3; i++) buf[__ind(row, col, img) + i + 1] = colours[i] / colour_count[i]; // Take the average of all colours in the pixels neighbourhood.
            buf[__ind(row, col, img)] = __RGB_to_mono(buf[__ind(row, col, img) + 1], buf[__ind(row, col, img) + 2], buf[__ind(row, col, img) + 3]);
        }
    }

    // Write data frome buffer to image and calculate average.
    for (int row = 0; row < img->height; row++) for (int col = 0; col < img->width; col++){
        for (int c = 0; c < 4; c++) img->data[__ind(row, col, img) + c] = buf[__ind(row, col, img) + c];
        img->avg += ( double ) img->data[__ind(row, col, img)];
    }
    img->avg /= ( double ) (img->width*img->height);
    
    free(buf);
    return 0;
}

/// @brief Reads the contents of a file into a given picture struct.
/// @param path Should have .pgm or .fits extension.
/// @param RGB_to_mono RGB to monochrome conversion function.
/// @return -1 if file invalid, -2 if allocation failed, 0 otherwise.
int read_starfile (char const * path, starfile * img){
    img->file = fopen(path, "rb");
    if (img->file == NULL) return -1; // Invalid path.
    img->type = __check_file_type(path); // Check file type.

    // Get metadata.
    if (__read_metadata(img) == -1){
        __close(img, 0); return -2; // Invalid file.
    }
    int temp = img->max + 1, i; for (i = 0; temp > 1; i++) temp = temp >> 1; int bytepix = i >> 3; // Determine bytes per pixelvalue.

    // Get data.
    img->data = ( unsigned short * ) malloc(img->height * img->width * 4 * sizeof(unsigned short)); // Allocate row array.
    if (img->data == NULL) { __close(img, 0); return -3; } // Malloc failed.
    for (int row = 0; row < img->height; row++){
        for (int col = 0; col < img->width; col++){
            unsigned short temp = 0;
            if (fread(&temp, bytepix, 1, img->file) != 1){ // Read a pixel from the image.
                __close(img, 1); return -4; // EOF reached.
            }
            img->data[__ind(row, col, img)] = temp;
            img->avg += ( double ) img->data[__ind(row, col, img)];
        }
    }
    img->avg /= ( double ) (img->width*img->height);

    if (img->type == FITS && __debayer(img) == -1) // Debayer (only if file is FITS type).
        return -3; // Malloc failed.

    return 0;
}

/// @brief Checks if a bright pixel is part of a potential star.
/// @return 1 if it is, 0 otherwise.
int __potential_star (starfile * img, int x, int y){
    if (x+1 >= img->width || y+1 >= img->height) return 0; // Cut-off star or noise at the edge of the image.
    if (img->data[__ind(y+1, x, img)] > img->thres || img->data[__ind(y, x+1, img)] > img->thres) return 1; // Potential star.
    return 0; // Noise.
}

/// @brief Checks if a bright pixel is not too close to any known stars.
/// @return 0 if it is too close, 1 otherwise.
int __compare_star (star stars [], int x, int y, int stari){
    for (int i = 0; i < stari; i++)
        if (( double ) abs(stars[i].pos.x - x) < __STAR_MARGIN*stars[i].FWHM &&
            ( double ) abs(stars[i].pos.y - y) < __STAR_MARGIN*stars[i].FWHM) return 0; // Too close to known stars.

    return 1; // New star.
}

/// @brief Find the current star's center by taking the weighted average.
/// @return 1 if star was cutt-off or too large, 0 otherwise.
int __find_middle (starfile * img, star stars [], int x, int y, int stari){
    vect pos = {0};
    int xcur, ycur, max_size = __MAX_STARSIZE / 2;
    double s, c; // Sum and count variables.
    
    pos.x = x; pos.y = y;

    for (int iter = 0; iter < __FIND_MIDDLE_ITER; iter++){ // Iterate to find star center more reliably.
        xcur = pos.x; ycur = pos.y;

        // Find middle x
        s = 0.0; c = 0.0; int count = 0;
        while (img->data[__ind(__rd(pos.y), xcur, img)] > img->thres) { xcur--; if (xcur < 0) return 1;} xcur++; // Offset to edge of star.
        while (img->data[__ind(__rd(pos.y), xcur, img)] > img->thres) { // Find other edge of star.
            s += ( double ) (img->data[__ind(__rd(pos.y), xcur, img)] * xcur);
            c += ( double ) img->data[__ind(__rd(pos.y), xcur, img)];

            xcur++; count++;
            if (xcur >= img->width) return 1; // Cut-off star.
            if (count > max_size) return 1; // Star too big.
        }
        pos.x = s/c; // Take the weighted average to find the center.
        
        // Find middle y
        s = 0.0; c = 0.0; count = 0;
        while (img->data[__ind(ycur, __rd(pos.x), img)] > img->thres) { ycur--; if (ycur < 0) return 1;} ycur++; // Offset to edge of star.
        while (img->data[__ind(ycur, __rd(pos.x), img)] > img->thres) { // Find other edge of star.
            s += ( double ) (img->data[__ind(ycur, __rd(pos.x), img)] * ycur);
            c += ( double ) img->data[__ind(ycur, __rd(pos.x), img)];

            ycur++; count++;
            if (ycur >= img->height) return 1; // Cut-off star.
            if (count > max_size) return 1; // Star too big.
        }
        pos.y = s/c; // Take the weighted average to find the center.
    }
    stars[stari].pos = pos; // Store extracted star center.

    return 0;
}

/// @brief Calculates the distance from the center of the current star to the 'edge' in a given direction.
/// @param thres Defines the pixel value at the 'edge'of the star.
double __dist_ray (starfile * img, star stars [], double dir, int sign, int stari, int thres){
    double x = stars[stari].pos.x, y = stars[stari].pos.y,
    xinc = ( double ) sign * cos(dir*M_PI/180.0),
    yinc = ( double ) sign * sin(dir*M_PI/180.0);

    do {
        x += xinc; y += yinc;
        if (__rd(x) >= img->width || __rd(y) >= img->height || __rd(x) < 0 || __rd(y) < 0) break;
    } while (img->data[__ind(__rd(y), __rd(x), img)] > thres);

    double xdist = x - stars[stari].pos.x,
    ydist = y - stars[stari].pos.y;

    return sqrt(xdist*xdist + ydist*ydist);
}

/// @brief Finds the index in the distance array of the axis perpendicular to the axis with the given angle.
int __calc_perp_ax_index (double other_axis_angle){
    double axis_angle = 90.0 + other_axis_angle;
    if (axis_angle >= 180.0) axis_angle -= 180.0;
    int axis_index = axis_angle * __RAYS/180;
    return axis_index;
}

/// @brief Calculates the eccentricity of the current star assuming an ellipse.
double __find_eccentricity (starfile * img, star stars [], int stari){
    double dist_arr [__RAYS], max = 0;
    int i = 0, same_ma_ax_len = 0, mi_ax_index_arr [__RAYS];

    // Find semi-major axis length and direction.
    for (double dir = 0.0; dir < 180.0; dir += 180.0 / ( double ) __RAYS){
        dist_arr[i] = (__dist_ray(img, stars, dir, 1, stari, img->thres) + __dist_ray(img, stars, dir, -1, stari, img->thres))/2;

        if (dist_arr[i] > max) {
            max = dist_arr[i]; 
            mi_ax_index_arr[0] = __calc_perp_ax_index(dir); same_ma_ax_len = 1;
        } else if (max == dist_arr[i]){ // if two major axes are equal use the one with the smallest minor axis.
            mi_ax_index_arr[same_ma_ax_len] = __calc_perp_ax_index(dir); same_ma_ax_len++;
        }

        i++;
    }

    // Calculate index in 'dist' of the semi-minor axis length.
    int mi_axis_index = mi_ax_index_arr[0];
    for (i = 1; i < same_ma_ax_len; i++){
        if (dist_arr[mi_ax_index_arr[i]] < dist_arr[mi_axis_index]) mi_axis_index = mi_ax_index_arr[i];
    }

    stars[stari].angle =__calc_perp_ax_index(( double ) mi_axis_index * 180.0/__RAYS) * 180.0/__RAYS; // Calculate major axis angle.
    double e = sqrt(1 - ((dist_arr[mi_axis_index]*dist_arr[mi_axis_index]) / (max*max))); // Calculate eccentricity assuming an ellipse.

    return e;
}

/// @brief Calculates the FWHM of the current star.
/// @param perp Along major axis y/n.
double __find_FWHM (starfile * img, star stars [], int stari, int perp){
    double dir;
    int HM = (img->data[__ind(__rd(stars[stari].pos.y), __rd(stars[stari].pos.x), img)] + img->avg) / 2; // Half maximum.
    if (HM < img->avg*2) HM = img->avg*2; // Truncate HM to above twice the average pixel value.

    // Calculate direction of the axis (minor/major)
    if (perp) dir = __calc_perp_ax_index(stars[stari].angle) * 180.0 / ( double ) __RAYS;
    else dir = stars[stari].angle;

    return __dist_ray(img, stars, dir, 1, stari, HM) + __dist_ray(img, stars, dir, -1, stari, HM); // Calculate HWHM in both directions.
}

/// @brief Calculates HFR along a given direction.
double __find_HFR (starfile * img, star stars [], int stari, double dir, int sign){
    double x = stars[stari].pos.x, y = stars[stari].pos.y,
    xinc = ( double ) sign * cos(dir*M_PI/180.0),
    yinc = ( double ) sign * sin(dir*M_PI/180.0),
    tot = 0.0;

    // Find total flux.
    do {
        tot += ( double ) img->data[__ind(__rd(y), __rd(x), img)] - img->avg;
        x += xinc; y += yinc;

        if (__rd(x) >= img->width || __rd(y) >= img->height || __rd(x) < 0 || __rd(y) < 0) break;
    } while (img->data[__ind(__rd(y), __rd(x), img)] > img->thres);

    x = stars[stari].pos.x; y = stars[stari].pos.y;
    tot /= 2.0; // Half flux.

    // Find HFR.
    int part = 0;
    do {
        part += img->data[__ind(__rd(y), __rd(x), img)] - img->avg;
        x += xinc; y += yinc;
        if (part >= tot) break; // Half flux reached.

        if (__rd(x) >= img->width || __rd(y) >= img->height || __rd(x) < 0 || __rd(y) < 0) break;
    } while (img->data[__ind(__rd(y), __rd(x), img)] > img->thres);

    // Calculate distance.
    double xdist = ( double ) stars[stari].pos.x - x,
    ydist = ( double ) stars[stari].pos.y - y;

    return sqrt(xdist*xdist + ydist*ydist);
}

/// @brief Calculates the HFR of the current star.
/// @param perp Along major axis y/n.
double __find_HFD (starfile * img, star stars [], int stari, int perp){
    double dir, sum, tot;

    if (perp) dir = __calc_perp_ax_index(stars[stari].angle) * 180.0/__RAYS;
    else dir = stars[stari].angle;

    return __find_HFR(img, stars, stari, dir, 1) + __find_HFR(img, stars, stari, dir, -1); // Calculate HFR in both directions.
}

/// @brief Calculates the SNR of the current star, using the average pixel value as the noise value, and the star center as the signal value.
double __find_SNR (starfile * img, star stars [], int stari){
    return 10.0*(log10(( double ) img->data[__ind(__rd(stars[stari].pos.y), __rd(stars[stari].pos.x), img)]) - log10(img->avg));
}

/// @brief Extracts stars from the given file into the given array.
/// @param img Should be run through the "read" function first.
/// @param stars Should be the size of N_stars or bigger.
/// @param N_stars Maximum number of stars to extract.
/// @return Number of extracted stars.
int extract_stars (starfile * img, star stars [], int N_stars){
    int stari = 0; // Current index in stars array

    for (int row = 0; row < img->height; row++){
        for (int col = 0; col < img->width; col++){
            if (stari != -1 && stari >= N_stars) return N_stars;

            int star_bool = 0;
            if (img->data[__ind(row, col, img)] > img->thres) star_bool = __potential_star(img, col, row); // Check bright pixel is part of a non-cutoff star
            if (star_bool) star_bool = __compare_star(stars, col, row, stari); // Check if bright pixel is not too close to any extracted stars
            if (star_bool){
                if (__find_middle(img, stars, col, row, stari)) continue; // Iteratively find the center of the star
                stars[stari].e = __find_eccentricity(img, stars, stari); // Find eccentricity of star and major axis incline angle
                stars[stari].FWHM = sqrt(__find_FWHM(img, stars, stari, 0) * __find_FWHM(img, stars, stari, 1)); // Take geometric mean of FWHM along minor and major axes
                stars[stari].HFD = sqrt(__find_HFD(img, stars, stari, 0) * __find_HFD(img, stars, stari, 1)); // Take geometric mean of HFD along minor and major axes
                stars[stari].SNR = __find_SNR(img, stars, stari);
                stari++;
            }
        }
    }

    return stari;
}
