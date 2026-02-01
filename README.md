# SnS
The three main goals of this project are:
1. to analyse astronomy related image files;
2. to process these files by stacking and calibrating
3. and to further refine these files by sharpening, denoising, etc.

This will all be done from the command line for now but I do intend to build for a more graphical user interface in the future.
I intend to make this project open source and it will be developed in C.
# Guide
As of now I have only started implementation of goal 1 of the project and as such there will be more additions to this guide when I have implemented more features.
## Analysis
To analyse a single image file[^1] please type the following command in your command line:
[^1]: For now only FITS and PGM files are allowed and only single images can be analysed.
```Shell
./bin/main [path/to/input/file]
```
After a few seconds[^2] a user interface will pop up asking you to select an option out of the following:
[^2]: Of course this depends on how beefy your computer is and how big your files are but I have a relatively slow computer and with 4K resolution files I only have to wait about 1 second for the analysis to finish.
- File info;
- Histogram;
- Statistics;
- List stars;
- Mark stars
- or Quit.

Some of these are very self explanitory but I will still provide documentation for them here. Before that I want to explain what the program did in those few seconds before the UI pops up. The program uses the amount of memory needed to store the file you gave it and extracted all the stars from this file. Next it calculates the position of the centroid, eccentricity, inclination, full width at half maximum, half flux diamater and signal to noise ratio for all of the extracted stars. I might later add some more calculated parameters of the extracted stars and update the star detection algorithm. I would also like to add some functionality to distinguish stars from non-star objects eg. galaxy cores. Stars are currently detected by checking if a pixel is above a certain threshold value[^3] and will be rejected if they are too small or too large to rule out noise and non-star objects eg. nebulae. Noteworthy calculations are the FWHM, HFD and SNR. FWHM is of calculated as twice the distance between the centroid and half of the maximum of the star. The maximum I have picked to be the pixel value at center of the star, I might later change to more robustly find the brightest pixxel in the star. If half of the maximum is below twice the average of all the pixels in the image it will be set to that last value. This is to avoid inaccurate FWHM values for dim stars. Furthermore, both the FWHM and HFD are calculated as the geometric mean of these values along the minor- and major axes of the star (assuming an ellipse). The SNR value is calculated with the following formula:
$$SNR=10\cdot\log\left(\frac{\text{Star center value}}{\text{Average value of file}}\right).$$
The $10*\log(\ldots)$ part is just to convert the actual signal to noise *ratio* to decibels. Here, the star center value is the "signal" and the average value of file is the "noise". I might change the interpretation of these two quantities later but for now this gives good enough results.
[^3]: This is currently controlled in the code (the detection_threshold function at the top of the main.c file) but I wish to make this available through user input very soon. Or adopt a different star detection algorithm.
### File info
As the name suggests, this option will display some information about the file you gave the program. Most of this information will be metadata, so the amount of displayed quantities can vary depending on the amount of metadata stored in the provided file. As of now, this option can display the following:
- the file type;
- the image dimensions;
- the maximum pixel value
- and the resolution in "/px.

The second to last quantity is calculated based on the amount of bits per pixel or read directly from the file, thus this is the maximum *possible* pixel value and not the actual maximum per se. The resolution is calculated based on the extracted focal length and pixel size, according to the formula on the [astronomy.tools](https://astronomy.tools/calculators/ccd) website.
### Histogram
Selecting this option will find the minimum-, maximum-[^4] and standard deviation of the pixel values. It will then show these values along with the mean and plot a low resolution histogram with a logarithmic vertical axis. In the future, I intend to find a way to increase the resolution on the histogram, calculate the median pixel value and calculate all of these values per colour channel.
### Statistics
This option will show the amount of extracted stars, mean pixel value, star detection threshold[^4] and the average star statistics. Namely the following quantities:
[^4]: These values are also given as a percentage of the maximum possible pixel value.
- the eccentricity (assuming an ellipse);
- the inclination of the major axis in degrees;
- the FWHM in pixels and arcseconds;
- the HFD in pixels and arcseconds;
- and the SNR in decibels.

### List stars
This option prints all of the values that are printed by the [statistics](#statistics) option and the star position for all of the extracted stars. The stars are also numbered from the top left down and snaking left and right.

### Mark stars
This option generates a PPM file whith all the stars circled by a green circle with a diameter of twice the FWHM and the centroids are marked with a red dot. It is good to keep in mind that this red dot is not the actual calculated centroid position, being rounded to the nearest pixel. I do plan to make this option a little less scuffed in the future.

### Quit
This option does what it says on the tin, however it is worth noting that this option also deallocates all of the used memory. This is something that [CRTL+C] doesn't do, and it will therefore cause a memory leak.

## Stack
