/* DO NOT EDIT THIS FILE */

#include <math.h>
#include <png.h>
#include <stdlib.h>
#include <unistd.h>

#include "color.h"
#include "image.h"
#include "log.h"

image_t *image_create(size_t width, size_t height) {
  image_t *image = calloc(1, sizeof(*image));
  if (image == NULL) {
    LOG_ERROR_ERRNO("calloc");
    goto fail_exit;
  }

  image->width = width;
  image->height = height;

  image->pixels =
      malloc((image->width * image->height) * sizeof(*image->pixels));
  if (image->pixels == NULL) {
    LOG_ERROR_ERRNO("malloc");
    goto fail_free_image;
  }

  return image;

fail_free_image:
  free(image);
fail_exit:
  return NULL;
}

image_t *image_create_from_png(char *filename) {
  if (filename == NULL) {
    LOG_ERROR_NULL_PTR();
    goto fail_exit;
  }

  /* source: https://gist.github.com/niw/5963798 */

  FILE *file = fopen(filename, "rb");
  if (file == NULL) {
    LOG_ERROR_ERRNO("fopen");
    goto fail_exit;
  }

  png_structp png =
      png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (png == NULL) {
    LOG_ERROR("couldn't create png_struct");
    goto fail_close_file;
  }

  png_infop info = png_create_info_struct(png);
  if (info == NULL) {
    LOG_ERROR("couldn't create png_infop");
    goto fail_free_png_struct;
  }

  if (setjmp(png_jmpbuf(png))) {
    goto fail_free_png_info;
  }

  png_init_io(png, file);
  png_read_info(png, info);

  image_t *image = image_create(png_get_image_width(png, info),
                                png_get_image_height(png, info));
  if (image == NULL) {
    goto fail_free_png_info;
  }

  png_byte color = png_get_color_type(png, info);
  png_byte depth = png_get_color_type(png, info);

  /* read any color_type into 8 bit depth, RGBA format */

  if (depth == 16) {
    png_set_strip_16(png);
  }

  if (color == PNG_COLOR_TYPE_PALETTE) {
    png_set_palette_to_rgb(png);
  }

  /* PNG_COLOR_TYPE_GRAY_ALPHA is always 8 or 16 bit depth */

  if (color == PNG_COLOR_TYPE_GRAY && depth < 8) {
    png_set_expand_gray_1_2_4_to_8(png);
  }

  if (png_get_valid(png, info, PNG_INFO_tRNS)) {
    png_set_tRNS_to_alpha(png);
  }

  /* these color_type don't have an alpha channel then fill it with 0xff */

  if (color == PNG_COLOR_TYPE_RGB || color == PNG_COLOR_TYPE_GRAY ||
      color == PNG_COLOR_TYPE_PALETTE) {
    png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
  }

  if (color == PNG_COLOR_TYPE_GRAY || color == PNG_COLOR_TYPE_GRAY_ALPHA) {
    png_set_gray_to_rgb(png);
  }

  png_read_update_info(png, info);

  /* read image data */

  png_bytep *row_pointers = calloc(image->height, sizeof(*row_pointers));
  if (row_pointers == NULL) {
    goto fail_free_image;
  }

  for (int j = 0; j < image->height; j++) {
    row_pointers[j] = malloc(png_get_rowbytes(png, info));
    if (row_pointers[j] == NULL) {
      goto fail_free_rows;
    }
  }

  png_read_image(png, row_pointers);

  /* copy image data */

  for (int j = 0; j < image->height; j++) {
    for (int i = 0; i < image->width; i++) {
      pixel_t *pixel = image_get_pixel(image, i, j);
      pixel->bytes[0] = row_pointers[j][4 * i + 0];
      pixel->bytes[1] = row_pointers[j][4 * i + 1];
      pixel->bytes[2] = row_pointers[j][4 * i + 2];
      pixel->bytes[3] = row_pointers[j][4 * i + 3];
    }
  }

  /* cleanup */

  for (int j = 0; j < image->height; j++) {
    if (row_pointers[j] != NULL) {
      free(row_pointers[j]);
    }
  }
  free(row_pointers);

  png_destroy_read_struct(&png, &info, NULL);
  fclose(file);

  return image;

fail_free_rows:
  for (int j = 0; j < image->height; j++) {
    if (row_pointers[j] != NULL) {
      free(row_pointers[j]);
    }
  }
  free(row_pointers);
fail_free_image:
  image_destroy(image);
fail_free_png_info:
  png_destroy_read_struct(&png, &info, NULL);
fail_free_png_struct:
  png_destroy_read_struct(&png, NULL, NULL);
fail_close_file:
  fclose(file);
fail_exit:
  return NULL;
}

void image_destroy(image_t *image) {
  if (image->pixels != NULL) {
    free(image->pixels);
  }
  free(image);
}

int image_save_png(image_t *image, char *filename) {
  if (image == NULL || filename == NULL) {
    LOG_ERROR_NULL_PTR();
    goto fail_exit;
  }

  /* source: https://gist.github.com/niw/5963798 */

  FILE *file = fopen(filename, "wb");
  if (file == NULL) {
    LOG_ERROR_ERRNO("fopen");
    goto fail_exit;
  }

  png_structp png =
      png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (png == NULL) {
    LOG_ERROR("couldn't create png_struct");
    goto fail_close_file;
  }

  png_infop info = png_create_info_struct(png);
  if (info == NULL) {
    LOG_ERROR("couldn't create png_infop");
    goto fail_free_png_struct;
  }

  if (setjmp(png_jmpbuf(png))) {
    goto fail_free_png_info;
  }

  png_init_io(png, file);

  /* output is 8 bit depth, RGBA format */

  png_set_IHDR(png, info, image->width, image->height, 8, PNG_COLOR_TYPE_RGBA,
               PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
               PNG_FILTER_TYPE_DEFAULT);

  png_write_info(png, info);

  /* copy image data */

  png_bytep *row_pointers = calloc(image->height, sizeof(*row_pointers));
  if (row_pointers == NULL) {
    goto fail_free_png_struct;
  }

  for (int j = 0; j < image->height; j++) {
    row_pointers[j] = malloc(png_get_rowbytes(png, info));
    if (row_pointers[j] == NULL) {
      goto fail_free_rows;
    }
  }

  for (int j = 0; j < image->height; j++) {
    for (int i = 0; i < image->width; i++) {
      pixel_t *pixel = image_get_pixel(image, i, j);
      row_pointers[j][4 * i + 0] = pixel->bytes[0];
      row_pointers[j][4 * i + 1] = pixel->bytes[1];
      row_pointers[j][4 * i + 2] = pixel->bytes[2];
      row_pointers[j][4 * i + 3] = pixel->bytes[3];
    }
  }

  /* write PNG file */

  png_write_image(png, row_pointers);
  png_write_end(png, NULL);

  /* cleanup */

  for (int j = 0; j < image->height; j++) {
    if (row_pointers[j] != NULL) {
      free(row_pointers[j]);
    }
  }
  free(row_pointers);

  png_destroy_write_struct(&png, &info);
  fclose(file);

  return 0;

fail_free_rows:
  for (int j = 0; j < image->height; j++) {
    if (row_pointers[j] != NULL) {
      free(row_pointers[j]);
    }
  }
  free(row_pointers);
fail_free_png_info:
  png_destroy_write_struct(&png, &info);
  goto fail_close_file;
fail_free_png_struct:
  png_destroy_write_struct(&png, NULL);
fail_close_file:
  fclose(file);
fail_exit:
  return -1;
}

image_t *image_from_grid(grid_t *grid) {
  if (grid == NULL) {
    LOG_ERROR_NULL_PTR();
    goto fail_exit;
  }

  image_t *image = image_create(grid->width, grid->height);
  if (image == NULL) {
    LOG_ERROR("failed to create image");
    goto fail_exit;
  }

  double max = grid_max(grid);
  if (isnan(max)) {
    LOG_ERROR("failed to get grid's max");
    goto fail_destroy_image;
  }

  for (unsigned int j = 0; j < grid->height; j++) {
    for (unsigned int i = 0; i < grid->width; i++) {
      pixel_t *pixel = image_get_pixel(image, i, j);
      double value = *grid_get_cell(grid, i, j);
      color_value(pixel, value, max);
    }
  }

  return image;

fail_destroy_image:
  image_destroy(image);
fail_exit:
  return NULL;
}

grid_t *image_to_grid(image_t *image, int channel) {
  if (image == NULL) {
    LOG_ERROR_NULL_PTR();
    goto fail_exit;
  }

  if (channel >= 3) {
    LOG_ERROR("invalid channel specified");
    goto fail_exit;
  }

  grid_t *grid = grid_create(image->width, image->height, 0);
  if (grid == NULL) {
    LOG_ERROR("failed to create grid");
    goto fail_exit;
  }

  unsigned int division = 255 * 255;
  for (unsigned int j = 0; j < image->height; j++) {
    for (unsigned int i = 0; i < image->width; i++) {
      pixel_t *pixel = image_get_pixel(image, i, j);
      *grid_get_cell(grid, i, j) =
          (pixel->bytes[channel] * pixel->bytes[3]) / division;
    }
  }

  return grid;

fail_exit:
  return NULL;
}
