#ifndef FILESYNC
#define FILESYNC

#include <unistd.h>
#include <stdbool.h>

bool is_directory(const char *path);
bool path_contains(const char *path1, const char *path2);
void run_filesync(const char* src, const char* dst, bool is_recursive, off_t size_threshold);

#endif
