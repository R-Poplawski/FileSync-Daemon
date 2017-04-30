#include "filesync.h"
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>

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
        char pth[4096];
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
