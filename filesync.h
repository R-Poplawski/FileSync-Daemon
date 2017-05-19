#ifndef FILESYNC
#define FILESYNC

#include <unistd.h>
#include <stdbool.h>

typedef enum file_type
{
    FT_NONE,
    FT_REGULAR,
    FT_DIRECTORY,
    FT_OTHER
} file_type;

bool path_contains(const char *path1, const char *path2);
file_type get_file_type(const char *path);
void run_filesync(const char *src, const char *dst, bool is_recursive, off_t size_threshold);

#endif
