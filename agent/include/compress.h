#ifndef COMPRESS_H
#define COMPRESS_H

#include <stddef.h>

int gzip_compress(const char *input, size_t input_size, unsigned char **output, size_t *output_size);

#endif