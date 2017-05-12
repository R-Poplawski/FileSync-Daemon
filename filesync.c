#include "filesync.h"
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <limits.h>

void writeToLog(const char *str);

typedef enum file_type
{
    FT_NONE,
    FT_REGULAR,
    FT_DIRECTORY,
    FT_OTHER
} file_type;

file_type get_file_type(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0) return FT_NONE;
    if (S_ISREG(st.st_mode)) return FT_REGULAR;
    if (S_ISDIR(st.st_mode)) return FT_DIRECTORY;
    return FT_OTHER;
}

time_t get_mtime(const char *path)
{
    struct stat statbuf;
    if (stat(path, &statbuf) == -1)
    {
        perror(path);
        return -1;
    }
    return statbuf.st_mtime;
}

off_t get_size(const char *path)
{
    struct stat statbuf;
    if (stat(path, &statbuf) == -1)
    {
        perror(path);
        return -1;
    }
    return statbuf.st_size;
}

bool is_directory(const char *path)
{
	struct stat sb;
	return (stat(path, &sb) == 0 && S_ISDIR(sb.st_mode));
}

bool path_contains(const char *path1, const char *path2)
{
    int len1 = strlen(path1), len2 = strlen(path2);
    if (len1 >= len2) return false;
    char pth1[PATH_MAX];
    strcpy(pth1, path1);
    if (pth1[len1 - 1] != '/') strcat(pth1, "/");
    return (strncmp(pth1, path2, strlen(pth1)) == 0);
}

void compare_directories(const char *src, const char *dst, bool recursive, ssize_t size_threshold)
{
    DIR *src_dir = opendir(src);
    if (src_dir == NULL)
    {
        char str[PATH_MAX + 30];
        snprintf(str, sizeof(str), "Failed opening directory (%s)\n", src);
        writeToLog(str);
        return;
    }
    struct dirent *src_ent;
    while ((src_ent = readdir(src_dir)) != NULL)
    {
        if (strcmp(src_ent->d_name, ".") == 0 || strcmp(src_ent->d_name, "..") == 0) continue;
        
        char src_ent_path[PATH_MAX], dst_ent_path[PATH_MAX];
        snprintf(src_ent_path, sizeof(src_ent_path), "%s/%s", src, src_ent->d_name);
        snprintf(dst_ent_path, sizeof(dst_ent_path), "%s/%s", dst, src_ent->d_name);
              
        time_t src_mtime = get_mtime(src_ent_path);
        char ts[32];
        strftime(ts, sizeof(ts), "%Y/%m/%d %H:%M:%S", localtime(&src_mtime));
        char str[PATH_MAX + 40];
        snprintf(str, sizeof(str), "%s \"%s\"", ts, src_ent_path);

        if (recursive && src_ent->d_type == DT_DIR) // directory
        {
            char s[PATH_MAX + 40];
            snprintf(s, sizeof(s), "D: %s\n", str);
            writeToLog(s);
            file_type dst_ft = get_file_type(dst_ent_path);
            switch (dst_ft)
            {
                case FT_DIRECTORY:
                    writeToLog("Destination directory exists\n");
                    compare_directories(src_ent_path, dst_ent_path, true, size_threshold);
                    break;
                case FT_NONE:
                    writeToLog("Destination directry doesn't exist\n");
                    break;
                default:
                    writeToLog("Other type named like the source directory exists at the destination\n");
                    break;
            }
        }
        else if (src_ent->d_type == DT_REG) // regular file
        {
            off_t src_size = get_size(src_ent_path);
            char s[PATH_MAX + 40];
            snprintf(s, sizeof(s), "F: %s size: %llu (%s)\n", str, (unsigned long long)src_size, (src_size <= size_threshold ? "doesn't exceed threshold" : "exceeds threshold"));
            writeToLog(s);
            file_type dst_ft = get_file_type(dst_ent_path);
            switch (dst_ft)
            {
                case FT_REGULAR:
                    {
                        time_t dst_mtime = get_mtime(dst_ent_path);
                        char dst_ts[32];
                        strftime(dst_ts, sizeof(dst_ts), "%Y/%m/%d %H:%M:%S", localtime(&dst_mtime));
                        snprintf(str, sizeof(str), "File exists at the destination (%s)\n", (dst_mtime == src_mtime ? "same modification time" : "different modification time"));
                        writeToLog(str);
                    }
                    break;
                case FT_NONE:
                    writeToLog("File doesn't exist at the destination\n");
                    break;
                default:
                    writeToLog("Other type named like the source file exists at the destination\n");
                    break;
            }
        }
    }
    closedir(src_dir);
}

int remove_directory(const char *path)
{
    char str[PATH_MAX + 30];
    snprintf(str, sizeof(str), "Directory to remove: %s\n", path);
    writeToLog(str);

    DIR *dir = opendir(path);
    if (dir == NULL) return -1;

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL)
    {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;

        char ent_path[PATH_MAX];
        snprintf(ent_path, sizeof(ent_path), "%s/%s", path, ent->d_name);

        switch (ent->d_type)
        {
            case DT_DIR:
                {
                    int res = remove_directory(ent_path);
                    if (res != 0)
                    {
                        closedir(dir);
                        return res;
                    }
                }
                break;
            case DT_REG:
                snprintf(str, sizeof(str), "File to remove: %s\n", ent_path);
                writeToLog(str);
                // TODO: Remove file
                break;
            default:
                snprintf(str, sizeof(str), "Other type in directory to remove: %s\n", ent_path);
                writeToLog(str);
                closedir(dir);
                return -2;
        }
    }
    closedir(dir);
    // TODO: Remove directory at path
    return 0;
}

void remove_extras(const char *src, const char *dst, bool recursive)
{
    DIR *dst_dir = opendir(dst);
    if (dst_dir == NULL)
    {
        char str[PATH_MAX + 30];
        snprintf(str, sizeof(str), "Failed opening directory (%s)\n", dst);
        writeToLog(str);
        return;
    }
    struct dirent *dst_ent;
    while ((dst_ent = readdir(dst_dir)) != NULL)
    {
        if (strcmp(dst_ent->d_name, ".") == 0 || strcmp(dst_ent->d_name, "..") == 0) continue;

        char src_ent_path[PATH_MAX], dst_ent_path[PATH_MAX];
        snprintf(src_ent_path, sizeof(src_ent_path), "%s/%s", src, dst_ent->d_name);
        snprintf(dst_ent_path, sizeof(dst_ent_path), "%s/%s", dst, dst_ent->d_name);

        time_t t = get_mtime(dst_ent_path);
        char ts[32];
        strftime(ts, sizeof(ts), "%Y/%m/%d %H:%M:%S", localtime(&t));
        char str[PATH_MAX + 40];
        snprintf(str, sizeof(str), "%s \"%s\"\n", ts, dst_ent_path);

        if (recursive && dst_ent->d_type == DT_DIR) // directory
        {
            char s[PATH_MAX + 40] = "D: ";
            strcat(s, str);
            writeToLog(s);
            file_type src_ft = get_file_type(src_ent_path);
            switch (src_ft)
            {
                case FT_DIRECTORY:
                    writeToLog("Source directory exists\n");
                    remove_extras(src_ent_path, dst_ent_path, true);
                    break;
                case FT_NONE:
                    {
                        writeToLog("Source directory doesn't exist\n");
                        int res = remove_directory(dst_ent_path);
                        if (res == 0) snprintf(s, sizeof(s), "Directory removed (%s)\n", dst_ent_path);
                        else if (res == -1) snprintf(s, sizeof(s), "Failed removing directory (%s), couldn't open directory\n", dst_ent_path);
                        else if (res == -2) snprintf(s, sizeof(s), "Failed removing directory (%s), other file type exists in the directory\n", dst_ent_path);
                        writeToLog(s);
                    }
                    break;
                default:
                    writeToLog("Other type named like the destination directory exists at the source\n");
                    break;
            }
        }
        else if (dst_ent->d_type == DT_REG) // regular file
        {
            char s[PATH_MAX + 40] = "F: ";
            strcat(s, str);
            writeToLog(s);
            file_type src_ft = get_file_type(src_ent_path);
            switch (src_ft)
            {
                case FT_REGULAR:
                    writeToLog("Source file exists\n");
                    break;
                case FT_NONE:
                    writeToLog("Source file doesn't exist\n");
                    break;
                default:
                    writeToLog("Other type named like the destination file exists at the source\n");
                    break;
            }
        }
    }
    closedir(dst_dir);
}

void run_filesync(const char *src, const char *dst, bool is_recursive, off_t size_threshold)
{
    char str[256];
    snprintf(str, sizeof(str), "run_filesync(\"%s\", \"%s\", %s, %zu)\n", src, dst, is_recursive ? "true" : "false", size_threshold);
    writeToLog(str);
    writeToLog("Searching for files to remove from destination that don't exist at source\n");
    remove_extras(src, dst, is_recursive);
    writeToLog("Searching for files to copy from source to destination\n");
    compare_directories(src, dst, is_recursive, size_threshold);
}
