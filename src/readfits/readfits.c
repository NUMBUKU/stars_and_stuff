# include "readfits.h"

char const * __BAYERPAT = "BAYERPAT";
char const * __END      = "END     ";

/// @brief Places the file pointer at the start of the data array.
/// @return -1 if the END keyword cannot be found or file ended preemptively, 0 otherwise.
int goto_end (FILE * fptr){
    int read_counter = 0, endbool;
    char c;

    while (1) {
        endbool = 1;
        for (int i = 0; i < 8; i++){ // Read keyword.
            c = fgetc(fptr);
            if (c == EOF) return -1;
            if (c != __END[i]) endbool = 0;
        }
        for (int i = 8; i < 80; i++) if (fgetc(fptr) == EOF) return -1; // Clear the lagging " "'s after keyword.

        read_counter++; // Increment amount of chunks we have read.
        read_counter = read_counter % 36; // Go back to 0 after block of 36 keyvals has ended.

        if (!endbool) continue; // Did not reach end.

        for (int i = read_counter; i < 36; i++) // Loop through remaining blocks in HDU.
            for (int j = 0; j < 80; j++) // Loop through block.
                if (fgetc(fptr) == EOF) return -1; // Clear the remaining " "'s.
        break;
    }

    return 0;
}

/// @brief Finds the given keyword.
/// @return -1 if keyword cannot be found, 0 otherwise.
int __find_keyword (FILE * fptr, char const keyword [9]){
    while (1){
        int endbool = 1, keywordbool = 1;
        for (int i = 0; i < 8; i++){ // Read keyword
            char c = fgetc(fptr);
            if (c == EOF) return -1;
            if (c != keyword[i]) keywordbool = 0;
            if (c != __END[i]) endbool = 0;
        }
        if (endbool) return -1; // Could not find keyword
        if (keywordbool) break; // Found keyword
        fseek(fptr, 72, SEEK_CUR); // Go to next keyword
    }
    
    return 0;
}

/// @brief Reads the value corresponding to the current keyword.
/// @return This value.
float __read_float_keyval (FILE * fptr){
    int counter = 0, temp, pointpos = 0;
    float ret = 0.0;
    fgetc(fptr); fgetc(fptr); // Clear the leading "= "

    do { // Clear the leading " "'s
        temp = fgetc(fptr);
        counter++;
    } while (temp == ' ');

    do { // Read float
        if ('0' <= temp && temp <= '9'){
            ret *= 10.0; ret += ( float ) (temp-'0');
        } else if (temp == '.'){
            pointpos = counter;
        } else {
            ret = ( float ) temp;
            break;
        }
        temp = fgetc(fptr);
        counter++;
    } while (('0' <= temp && temp <= '9') || temp == '.');

    for (int i = counter; i < 70; i++) fgetc(fptr); // Clear the lagging " "'s.

    if (pointpos == 0) return ret;
    for (int i = 0; i < counter-1 - pointpos; i++) ret /= 10;
    return ret;
}

/// @brief Reads the value of the given keyword.
/// @return This value; NAN if the keyword cannot be found.
float read_keyval (FILE * fptr, char const keyword [9]){
    long start = ftell(fptr); float ret;
    fseek(fptr, 0, SEEK_SET);

    if (__find_keyword(fptr, keyword) == -1) return NAN;
    ret = __read_float_keyval(fptr);

    fseek(fptr, start, SEEK_SET);
    return ret;
}

/// @brief Extract bayer pattern from file.
/// @param bayer String to store bayer pattern.
/// @return -1 if BAYERPAT keyword cannot be found, 0 otherwise.
int read_bayer (FILE * fptr, char bayer [5]){
    long start = ftell(fptr); fseek(fptr, 0, SEEK_SET);

    if (__find_keyword(fptr, __BAYERPAT) == -1) return -1;

    fgetc(fptr); fgetc(fptr); fgetc(fptr); // Clear the leading "= '".
    fgets(bayer, 5, fptr); // Read bayer pattern.
    for (int i = 5; i < 70; i++) fgetc(fptr); // Clear the lagging " "'s.

    fseek(fptr, start, SEEK_SET);
    return 0;
}
