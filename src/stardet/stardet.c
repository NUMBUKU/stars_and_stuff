# include "stardet.h"

# ifndef M_PI
# define M_PI 3.14159265358979323846
# endif

// User variables
int const __FIND_MIDDLE_ITER = 3, // Number of iterations to find star center
__STAR_MARGIN = 30, // Minimum distance between stars
__RAYS = 10; // Number of rays to find major/minor axis (should ALWAYS be even)

// RGB to monochrome conversion function
int __RGB_to_mono (int R, int G, int B){
    return ( int ) (0.2126 * ( double ) R + 0.7152 * ( double ) G + 0.0722 * ( double ) B);
}

// Perform a byte swap on the input
signed short __endian_swap (unsigned short in){
    short b0 = (in & 0xff) << 8, b1 = in >> 8;
    return b0 | b1;
}

// Deallocates and closes file if an error ocurred
void __close (picture * img, int row, int arr){
    fclose(img->file);
    for (int i = 0; i < row; i++) free(img->data[i]);
    if (arr) free(img->data);
}

// Round function
int __rd (double x){
    return ( int ) round(x);
}

int __check_PGM (char const * path){
    char c = ' '; int i, pos = -1;

    // Find the last . in file extension
    for (i = 0; c != '\0'; i++){
        c = path[i];
        if (c == '.') pos = i;
    }

    if (pos == -1) return 0; // Could not find a .

    i = pos+1;
    if (path[i] == 'p' && path[i+1] == 'g' && path[i+2] == 'm' && path[i+3] == '\0') return 1; // PGM file
    else if (path[i] == 'f' && path[i+1] == 'i' && path[i+2] == 't' && path[i+3] == 's' && path[i+4] == '\0') return 0; // FITS file
    
    return -1; // Invalid file type
}

int __read_metadata (picture * img){
    if (img->PGM){
        if (fgetc(img->file) != 'P' || fgetc(img->file) != '5') return -1; fgetc(img->file); // Check PGM for validity
        fscanf(img->file, "%d %d", &img->width, &img->height);
        fscanf(img->file, "%d", &img->max);
        return 0;
    }

    if (read_keyval(img->file, "SIMPLE  ") != ( float ) 'T') return -1; // Check FITS for validity

    img->width = ( int ) read_keyval(img->file, "NAXIS1  ");
    img->height = ( int ) read_keyval(img->file, "NAXIS2  ");
    img->max = (1 << ( int ) read_keyval(img->file, "BITPIX  ")) - 1;

    return 0;
}

int __debayer (picture * img){
    int bias = ( int ) read_keyval(img->file, "BZERO   "),
    mono = 0;
    char bayer [5]; if (read_bayer(img->file, bayer) == -1) mono = 1; // Monochrome file handling
    img->avg = 0.0; // Recalculate average

    for (int row = 0; row < img->height; row++) for (int col = 0; col < img->width; col++){
        img->data[row][col] = bias + __endian_swap(img->data[row][col]); // Convert raw data to physical value
        img->avg += img->data[row][col];
    }
    img->avg /= ( double ) (img->width*img->height);
    if (mono) return 0;
    img->avg = 0.0;

    // Interpolation
    unsigned short * buf = ( unsigned short * ) malloc(img->height * img->width * sizeof(unsigned short)); // Allocate a buffer for debayered image
    if (buf == NULL) return -1; // Malloc failed
    for (int row = 0; row < img->height; row++){
        for (int col = 0; col < img->width; col++){
            int color_count [] = {0, 0, 0}, colors [] = {0, 0, 0};

            for (int currow = row-1; currow < row+2; currow++) for (int curcol = col-1; curcol < col+2; curcol++){
                if (currow >= img->height || currow < 0 || curcol >= img->width || curcol < 0) continue;
                int bayer_index = (curcol % 2) + 2*(currow % 2), color_index = 1;

                if (bayer[bayer_index] == 'R') color_index = 0;
                else if (bayer[bayer_index] == 'G') color_index = 1;
                else if (bayer[bayer_index] == 'B') color_index = 2; 
                color_count[color_index]++;
                colors[color_index] += img->data[currow][curcol];
            }

            for (int i = 0; i < 3; i++) colors[i] /= color_count[i];
            buf[row*img->width + col] = __RGB_to_mono(colors[0], colors[1], colors[2]);
        }
    }

    for (int row = 0; row < img->height; row++) for (int col = 0; col < img->width; col++){
        img->data[row][col] = buf[row*img->width + col];
        img->avg += ( double ) img->data[row][col];
    }
    img->avg /= ( double ) (img->width*img->height);
    
    free(buf);
    return 0;
}

// Get data from file
int read_starfile (char const * path, picture * img){
    img->file = fopen(path, "rb");
    if (img->file == NULL) return -1; // Invalid path
    img->PGM = __check_PGM(path); // Check if file is PGM or FITS type
    if (img->PGM == -1){ __close(img, 0, 0); return -2; } // Invalid file extension

    // Get metadata
    if (__read_metadata(img) == -1){
        __close(img, 0, 0); return -2; // Invalid file
    }
    int temp = img->max + 1, i; for (i = 0; temp > 1; i++) temp = temp >> 1; int bytepix = i >> 3; // Determine bytes per pixelvalue

    // Get data
    if (!img->PGM && goto_end(img->file) == -1){
        __close(img, 0, 0); return -2; // Invalid file
    }
    img->data = ( unsigned short ** ) malloc(img->height * sizeof(unsigned short *)); // Allocate row array
    if (img->data == NULL) { __close(img, 0, 0); return -3; } // Malloc failed
    for (int row = 0; row < img->height; row++){
        img->data[row] = ( unsigned short * ) malloc(img->width * sizeof(unsigned short)); // Allocate row
        if (img->data[row] == NULL) { __close(img, row, 1); return -3; } // Malloc failed
        for (int col = 0; col < img->width; col++){
            unsigned short temp = 0;
            if (fread(&temp, bytepix, 1, img->file) != 1){ // Read a pixel from the image
                __close(img, row+1, 1); return -4; // EOF reached
            }
            img->data[row][col] = temp;
            img->avg += ( double ) img->data[row][col];
        }
    }
    img->avg /= ( double ) (img->width*img->height);

    if (!img->PGM && __debayer(img) == -1) // Debayer (only if file is FITS type)
        return -3; // Malloc failed

    return 0;
}

// Check for false alarm
int __potential_star (picture * img, int x, int y){
    if (x+1 >= img->width || y+1 >= img->height) return 0; // Cut-off star or noise at the edge of the image
    if (img->data[y+1][x] > img->thres || img->data[y][x+1] > img->thres) return 1; // Star
    return 0; // Noise
}

// Check for duplicate star
int __compare_star (star stars [], int x, int y, int stari){
    for (int i = 0; i < stari; i++)
        if (abs(stars[i].pos.x - x) < 2.0*stars[i].FWHM && abs(stars[i].pos.y - y) < 2.0*stars[i].FWHM) return 0; // Reject if bright pixel is too close to known stars

    return 1; // New star
}

// Find the stars center and save it
int __find_middle (picture * img, star stars [], int x, int y, int stari){
    vect pos = {0};
    int xcur, ycur, max_size = __STAR_MARGIN / 2;
    double s, c;
    
    pos.x = x; pos.y = y;

    for (int iter = 0; iter < __FIND_MIDDLE_ITER; iter++){
        xcur = pos.x; ycur = pos.y;

        // Find middle x
        s = 0.0; c = 0.0; int count = 0;
        while (img->data[__rd(pos.y)][xcur] > img->thres) { xcur--; if (xcur < 0) return 1;} xcur++; // Offset to edge of star
        while (img->data[__rd(pos.y)][xcur] > img->thres) { // Find other edge of star
            s += ( double ) (img->data[__rd(pos.y)][xcur] * xcur);
            c += ( double ) img->data[__rd(pos.y)][xcur];

            xcur++; count++;
            if (xcur >= img->width) return 1; // Cut-off star
            if (count > max_size) return 1; // Star too big
        }
        pos.x = s/c;
        
        // Find middle y
        s = 0.0; c = 0.0; count = 0;
        while (img->data[ycur][__rd(pos.x)] > img->thres) { ycur--; if (ycur < 0) return 1;} ycur++; // Offset to edge of star
        while (img->data[ycur][__rd(pos.x)] > img->thres) { // Find other edge of star
            s += ( double ) (img->data[ycur][__rd(pos.x)] * ycur);
            c += ( double ) img->data[ycur][__rd(pos.x)];

            ycur++; count++;
            if (ycur >= img->height) return 1; // Cut-off star
            if (count > max_size) return 1; // Star too big
        }
        pos.y = s/c;
    }
    stars[stari].pos = pos;

    return 0;
}

// Calculate the distance to edge of the current star in a given diraction
double __dist_ray (picture * img, star stars [], double dir, int sign, int stari, int thres){
    double x = stars[stari].pos.x, y = stars[stari].pos.y,
    xinc = ( double ) sign * cos(dir*M_PI/180.0),
    yinc = ( double ) sign * sin(dir*M_PI/180.0);

    do {
        x += xinc; y += yinc;
        if (__rd(x) >= img->width || __rd(y) >= img->height || __rd(x) < 0 || __rd(y) < 0) break;
    } while (img->data[__rd(y)][__rd(x)] > thres);

    double xdist = x - stars[stari].pos.x,
    ydist = y - stars[stari].pos.y;

    return sqrt(xdist*xdist + ydist*ydist);
}

// Find the index of the axis perpendicular to the axis with the given angle
int __calc_perp_ax_index (double other_axis_angle){
    double axis_angle = 90.0 + other_axis_angle;
    if (axis_angle >= 180.0) axis_angle -= 180.0;
    int axis_index = axis_angle * __RAYS/180;
    return axis_index;
}

// Calculate the eccentricity of the current star assuming an ellipse
double __find_eccentricity (picture * img, star stars [], int stari){
    double dist_arr [__RAYS], max = 0;
    int i = 0, same_ma_ax_len = 0, mi_ax_index_arr [__RAYS];

    for (double dir = 0.0; dir < 180.0; dir += 180.0 / ( double ) __RAYS){ // Find semi-major axis length and direction
        dist_arr[i] = (__dist_ray(img, stars, dir, 1, stari, img->thres) + __dist_ray(img, stars, dir, -1, stari, img->thres))/2;

        if (dist_arr[i] > max) {
            max = dist_arr[i]; 
            mi_ax_index_arr[0] = __calc_perp_ax_index(dir); same_ma_ax_len = 1;
        } else if (max == dist_arr[i]){ // if two major axes are equal use the one with the smallest minor axis
            mi_ax_index_arr[same_ma_ax_len] = __calc_perp_ax_index(dir); same_ma_ax_len++;
        }

        i++;
    }

    // Calculate index in 'dist' of the semi-minor axis length
    int mi_axis_index = mi_ax_index_arr[0];
    for (i = 1; i < same_ma_ax_len; i++){
        if (dist_arr[mi_ax_index_arr[i]] < dist_arr[mi_axis_index]) mi_axis_index = mi_ax_index_arr[i];
    }

    stars[stari].angle =__calc_perp_ax_index(( double ) mi_axis_index * 180.0/__RAYS) * 180.0/__RAYS; // Calculate major axis angle 
    double e = sqrt(1 - ((dist_arr[mi_axis_index]*dist_arr[mi_axis_index]) / (max*max))); // Calculate eccentricity assuming an ellipse

    return e;
}

// Calculate FWHM assuming normal distribution
double __find_FWHM (picture * img, star stars [], int stari, int perp){
    double dir;
    int HM = (img->data[__rd(stars[stari].pos.y)][__rd(stars[stari].pos.x)] + img->avg) / 2; // Half maximum
    if (HM < img->avg*2) HM = img->avg*2;

    if (perp) dir = __calc_perp_ax_index(stars[stari].angle) * 180.0 / ( double ) __RAYS;
    else dir = stars[stari].angle;

    return __dist_ray(img, stars, dir, 1, stari, HM) + __dist_ray(img, stars, dir, -1, stari, HM);
}

double __find_HFR (picture * img, star stars [], int stari, double dir, int sign){
    double x = stars[stari].pos.x, y = stars[stari].pos.y,
    xinc = ( double ) sign * cos(dir*M_PI/180.0),
    yinc = ( double ) sign * sin(dir*M_PI/180.0),
    tot = 0.0;

    do {
        tot += ( double ) img->data[__rd(y)][__rd(x)] - img->avg;
        x += xinc; y += yinc;

        if (__rd(x) >= img->width || __rd(y) >= img->height || __rd(x) < 0 || __rd(y) < 0) break;
    } while (img->data[__rd(y)][__rd(x)] > img->thres);

    x = stars[stari].pos.x; y = stars[stari].pos.y;
    tot /= 2.0; int part = 0;

    do {
        part += img->data[__rd(y)][__rd(x)] - img->avg;
        x += xinc; y += yinc;
        if (part >= tot) break;

        if (__rd(x) >= img->width || __rd(y) >= img->height || __rd(x) < 0 || __rd(y) < 0) break;
    } while (img->data[__rd(y)][__rd(x)] > img->thres);

    double xdist = ( double ) stars[stari].pos.x - x,
    ydist = ( double ) stars[stari].pos.y - y;

    return sqrt(xdist*xdist + ydist*ydist);
}

double __find_HFD (picture * img, star stars [], int stari, int perp){
    double dir, sum, tot;

    if (perp) dir = __calc_perp_ax_index(stars[stari].angle) * 180.0/__RAYS;
    else dir = stars[stari].angle;

    return __find_HFR(img, stars, stari, dir, 1) + __find_HFR(img, stars, stari, dir, -1);
}

double __find_SNR (picture * img, star stars [], int stari){
    return 10.0*(log10(( double ) img->data[__rd(stars[stari].pos.y)][__rd(stars[stari].pos.x)]) - log10(img->avg));
}

// Extract star positions
int extract_stars (picture * img, star stars [], int N_stars){
    int stari = 0; // Current index in stars array

    for (int row = 0; row < img->height; row++){
        for (int col = 0; col < img->width; col++){
            if (stari != -1 && stari >= N_stars) return N_stars;

            int star_bool = 0;
            if (img->data[row][col] > img->thres) star_bool = __potential_star(img, col, row); // Check bright pixel is part of a non-cutoff star
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
