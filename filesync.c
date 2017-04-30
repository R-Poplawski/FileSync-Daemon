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

void get_mtime(const char *path, time_t *mtime)
{
    struct stat statbuf;
    if (stat(path, &statbuf) == -1)
    {
        perror(path);
        mtime = NULL;
    }
    *mtime = statbuf.st_mtime;
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

void listFiles(const char *path, bool recursive)
{
    DIR *dir = opendir(path);
    if (dir == NULL)
    {
        char str[4096];
        snprintf(str, sizeof(str), "Failed opening directory (%s)\n", path);
        writeToLog(str);
        return;
    }
    struct dirent *ent;
    while((ent = readdir(dir)) != NULL)
    {
        char pth[PATH_MAX];
        snprintf(pth, sizeof(pth), "%s/%s", path, ent->d_name);
        if (ent->d_type == DT_DIR && strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0) // directory
        {
            time_t t;
            get_mtime(pth, &t);
            char ts[32];
            strftime(ts, sizeof(ts), "%Y/%m/%d %H:%M:%S", localtime(&t));
            char str[350];
            snprintf(str, sizeof(str), "D: %s \"%s/%s\"\n", ts, path, ent->d_name);
            writeToLog(str);
            if (recursive) listFiles(pth, true);
        }
        else if (ent->d_type == DT_REG) // regular file
        {
            time_t t;
            get_mtime(pth, &t);
            char ts[32];
            strftime(ts, sizeof(ts), "%Y/%m/%d %H:%M:%S", localtime(&t));
            char str[350];
            snprintf(str, sizeof(str), "F: %s \"%s/%s\"\n", ts, path, ent->d_name);
            writeToLog(str);
        }
    }
    closedir(dir);
}

void run_filesync(const char* src, const char* dst, bool is_recursive, ssize_t size_threshold)
{
    char str[256];
    snprintf(str, sizeof(str), "run_filesync(\"%s\", \"%s\", %s, %zu)\n", src, dst, is_recursive ? "true" : "false", size_threshold);
    writeToLog(str);
    listFiles(src, is_recursive);
    listFiles(dst, is_recursive);
}
