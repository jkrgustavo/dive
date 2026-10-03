#include "util.h"

char *read_file(const char* path) {
    FILE *fptr;
    fptr = fopen(path, "r");
    if (!fptr) return NULL;

    int status = fseek(fptr, 0, SEEK_END);
    if (status != 0) return NULL;

    long size = ftell(fptr);
    if (size <= 0) return NULL;

    char *buffer = malloc(size + 1);
    rewind(fptr);

    size_t n = fread(buffer, 1, (size_t)size, fptr);
    buffer[n] = '\0';

    return buffer;
}
