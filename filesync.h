#ifndef FILESYNC
#define FILESYNC
#include <unistd.h>
#include <stdbool.h>

void run_filesync(char* src, char* dst, bool is_recursive, ssize_t size_treshold);

#endif
