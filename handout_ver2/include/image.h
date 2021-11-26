/* DO NOT EDIT THIS FILE */

#ifndef INCLUDE_IMAGE_H_
#define INCLUDE_IMAGE_H_

#include <stdbool.h>
#include <stddef.h>

#include "grid.h"
#include "pixel.h"

typedef struct image {
    size_t width;
    size_t height;
    pixel_t* pixels;
} image_t;

static inline pixel_t* image_get_pixel(image_t* image, unsigned int x, unsigned int y) {
    if (x >= image->width || y >= image->height) {
        return NULL;
    }

    return &image->pixels[x + y * image->width];
}

image_t* image_create(size_t width, size_t height);
image_t* image_create_from_png(char* filename);
void image_destroy(image_t* image);
int image_save_png(image_t* image, char* filename);

image_t* image_from_grid(grid_t* grid);
grid_t* image_to_grid(image_t* image, int channel);

#endif /* INCLUDE_IMAGE_H_ */