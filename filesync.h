#ifndef FILESYNC
#define FILESYNC

#include <unistd.h>
#include <stdbool.h>

void run_filesync(const char* src, const char* dst, bool is_recursive, ssize_t size_threshold);

#endif
