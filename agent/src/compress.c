#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "compress.h"

int gzip_compress(
    const char *input,
    size_t input_size,
    unsigned char **output,
    size_t *output_size
) {
    z_stream stream = {0};

    if (deflateInit2(
            &stream,
            Z_DEFAULT_COMPRESSION,
            Z_DEFLATED,
            15 + 16,    // gzip framing
            8,
            Z_DEFAULT_STRATEGY
        ) != Z_OK) {
        return -1;
    }

    size_t capacity = deflateBound(&stream, input_size);

    unsigned char *buffer = malloc(capacity);
    if (!buffer) {
        deflateEnd(&stream);
        return -1;
    }

    stream.next_in = (Bytef *)input;
    stream.avail_in = input_size;

    stream.next_out = buffer;
    stream.avail_out = capacity;

    int result = deflate(&stream, Z_FINISH);

    if (result != Z_STREAM_END) {
        free(buffer);
        deflateEnd(&stream);
        return -1;
    }

    *output_size = stream.total_out;
    *output = buffer;

    deflateEnd(&stream);

    return 0;
}