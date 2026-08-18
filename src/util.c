#include "util.h"

int read_file(char* buff, size_t len, const char* path) {
    FILE *fptr;
    fptr = fopen(path, "r");
    if (!fptr) return 0;

    int status = fseek(fptr, 0, SEEK_END);
    if (status != 0) return 0;

    long size = ftell(fptr);
    if (size <= 0 || size >= (long)len) return 0;
    rewind(fptr);

    size_t n = fread(buff, 1, (size_t)size, fptr);
    buff[n] = '\0';

    return 1;
}
