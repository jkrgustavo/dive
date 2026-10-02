#include "util.h"

char *read_file(const char* path) {
    FILE *fptr;
    fptr = fopen(path, "r");
    if (!fptr) return 0;

    int status = fseek(fptr, 0, SEEK_END);
    if (status != 0) return NULL;

    long size = ftell(fptr);
    char *buffer = malloc(size);
    if (size <= 0) return NULL;
    rewind(fptr);

    size_t n = fread(buffer, 1, (size_t)size, fptr);
    buffer[n] = '\0';

    return buffer;
}
