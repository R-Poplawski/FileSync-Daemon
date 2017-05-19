#include "filesync.h"
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <utime.h>
#include <limits.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/mman.h>

void writeToLog(const char *str);

file_type get_file_type(const char *path) // sprawdź typ pliku
{
    struct stat st;
    if (stat(path, &st) != 0) return FT_NONE;
    if (S_ISREG(st.st_mode)) return FT_REGULAR;
    if (S_ISDIR(st.st_mode)) return FT_DIRECTORY;
    return FT_OTHER;
}

time_t get_atime(const char *path) // sprawdź czas ostatniego dostępu do pliku
{
    struct stat statbuf;
    if (stat(path, &statbuf) == -1)
    {
        perror(path);
        return -1;
    }
    return statbuf.st_mtime;
}

time_t get_mtime(const char *path) // sprawdź czas modyfikacji pliku
{
    struct stat statbuf;
    if (stat(path, &statbuf) == -1)
    {
        perror(path);
        return -1;
    }
    return statbuf.st_mtime;
}

int set_mtime(const char *path, time_t mtime) // ustaw czas modyfikacji pliku
{
    struct utimbuf utb;
    utb.modtime = mtime;
    utb.actime = get_atime(path);
    return utime(path, &utb);
}

off_t get_size(const char *path) // sprawdź rozmiar pliku
{
    struct stat statbuf;
    if (stat(path, &statbuf) == -1)
    {
        perror(path);
        return -1;
    }
    return statbuf.st_size;
}

bool path_contains(const char *path1, const char *path2) // sprawdź czy katalog o ścieżce path1 zawiera element o ścieżce path2
{
    int len1 = strlen(path1), len2 = strlen(path2);
    if (len1 >= len2) return false;
    char pth1[PATH_MAX];
    strcpy(pth1, path1);
    if (pth1[len1 - 1] != '/') strcat(pth1, "/");
    return (strncmp(pth1, path2, strlen(pth1)) == 0);
}

int copy_rw(const char *src_ent_path, const char *dst_ent_path)
{
    int src_fd, dst_fd;
    ssize_t size_src, size_dst;

    src_fd = open(src_ent_path, O_RDONLY);
    if (src_fd == -1)
    {
        return -1;
    }
    dst_fd = open(dst_ent_path, O_WRONLY | O_TRUNC | O_CREAT, 0644);
    if (src_fd == -1) 
    {
        close(src_fd);
        return -2;
    }

    int BUF_SIZE = 16384;
    char *buffer = (char*)malloc(BUF_SIZE);

    while ((size_src = read(src_fd, buffer, BUF_SIZE)) > 0)
    {
        size_dst = write(dst_fd, buffer, (ssize_t)size_src);
        if (size_dst != size_src)
        {
            close(src_fd);
            close(dst_fd);
            free(buffer);
            return -3;
        }
    }

    close(src_fd);
    close(dst_fd);
    free(buffer);
    return 0;
}

int copy_mmap(const char *src_ent_path, const char *dst_ent_path)
{
    struct stat st;
    int src_fd, dst_fd;
    char *buffer;
    ssize_t size_dst;
    
    src_fd = open(src_ent_path, O_RDONLY);
    if (src_fd == -1)
    {
        return -1;
    }
    dst_fd = open(dst_ent_path, O_WRONLY | O_TRUNC | O_CREAT, 0644);
    if (dst_fd == -1)
    {
        close(src_fd);
        return -2;
    }

    fstat(src_fd, &st);
    buffer = mmap(0, st.st_size, PROT_READ, MAP_SHARED, src_fd, 0);
    size_dst = write(dst_fd, buffer, st.st_size);

    close(src_fd);
    close(dst_fd);
    return 0;
}

void copy_file(const char *src, const char *dst, bool use_mmap)
{
    int res = (use_mmap ? copy_mmap(src, dst) : copy_rw(src, dst));
    switch (res)
    {
        case 0:
            writeToLog("File copied\n");
            break;
        case -1:
            writeToLog("Source file couldn't be opened\n");
            return;
        case -2:
            writeToLog("Destination file couldn't be opened\n");
            return;
        case -3:
            writeToLog("Error while writing to file\n");
            return;
        default:
            writeToLog("Couldn't copy file\n");
            return;
    }
    time_t src_mtime = get_mtime(src);
    if (set_mtime(dst, src_mtime) != 0) 
    {
        writeToLog("Failed to change modification time\n");
        return;
    }
    writeToLog("Modification time changed\n");
}

void copy_directory(const char *src, const char *dst, off_t size_threshold)
{
    if (mkdir(dst, 0644) != 0) // spróbuj utworzyć katalog docelowy
    {
        writeToLog("Couldn't create a directory at the destination\n");
        return;
    }
    writeToLog("Directory created\n");
    
    DIR *src_dir = opendir(src); // otwórz katalog źródłowy
    if (src_dir == NULL)
    {
        char str[PATH_MAX + 30];
        snprintf(str, sizeof(str), "Failed opening directory (%s)\n", src);
        writeToLog(str);
        return;
    }

    struct dirent *src_ent;
    while ((src_ent = readdir(src_dir)) != NULL) // przeglądaj elementy w katalogu źródłowym
    {
        if (strcmp(src_ent->d_name, ".") == 0 || strcmp(src_ent->d_name, "..") == 0) continue;
        
        char src_ent_path[PATH_MAX], dst_ent_path[PATH_MAX];
        snprintf(src_ent_path, sizeof(src_ent_path), "%s/%s", src, src_ent->d_name); // ścieżka elementu źródłowego
        snprintf(dst_ent_path, sizeof(dst_ent_path), "%s/%s", dst, src_ent->d_name); // ścieżka elementu docelowego

        if (src_ent->d_type == DT_DIR) // element źródłowy jest katalogiem
        {
            copy_directory(src_ent_path, dst_ent_path, size_threshold);
        }
        else if (src_ent->d_type == DT_REG) // element źródłowy jest zwykłym plikiem
        {
            off_t src_size = get_size(src_ent_path);
            copy_file(src_ent_path, dst_ent_path, src_size > size_threshold);
        }
    }
    closedir(src_dir);
}

void compare_directories(const char *src, const char *dst, bool recursive, off_t size_threshold)
{
    DIR *src_dir = opendir(src); // otwórz katalog źródłowy
    if (src_dir == NULL)
    {
        char str[PATH_MAX + 30];
        snprintf(str, sizeof(str), "Failed opening directory (%s)\n", src);
        writeToLog(str);
        return;
    }
    struct dirent *src_ent;
    while ((src_ent = readdir(src_dir)) != NULL) // przeglądaj zawartość katalogu źródłowego
    {
        if (strcmp(src_ent->d_name, ".") == 0 || strcmp(src_ent->d_name, "..") == 0) continue;
        
        char src_ent_path[PATH_MAX], dst_ent_path[PATH_MAX];
        snprintf(src_ent_path, sizeof(src_ent_path), "%s/%s", src, src_ent->d_name); // ścieżka elementu źródłowego
        snprintf(dst_ent_path, sizeof(dst_ent_path), "%s/%s", dst, src_ent->d_name); // ścieżka elementu docelowego
              
        time_t src_mtime = get_mtime(src_ent_path);
        char ts[32];
        strftime(ts, sizeof(ts), "%Y/%m/%d %H:%M:%S", localtime(&src_mtime));
        char str[PATH_MAX + 40];
        snprintf(str, sizeof(str), "%s \"%s\"", ts, src_ent_path);

        if (recursive && src_ent->d_type == DT_DIR) // element źródłowy jest katalogiem
        {
            char s[PATH_MAX + 40];
            snprintf(s, sizeof(s), "D: %s\n", str);
            writeToLog(s);
            file_type dst_ft = get_file_type(dst_ent_path); // sprawdź typ elementu w katalogu docelowym
            switch (dst_ft)
            {
                case FT_DIRECTORY: // podkatalog docelowy istnieje
                    writeToLog("Destination directory exists\n");
                    compare_directories(src_ent_path, dst_ent_path, true, size_threshold);
                    break;
                case FT_NONE: // podkatalog docelowy nie istnieje
                    writeToLog("Destination directory doesn't exist\n");
                    copy_directory(src_ent_path, dst_ent_path, size_threshold);
                    break;
                default: // element docelowy jest innego typu niż element źródłowy
                    writeToLog("Other type named like the source directory exists at the destination\n");
                    break;
            }
        }
        else if (src_ent->d_type == DT_REG) // element źródłowy jest zwykłym plikiem
        {
            off_t src_size = get_size(src_ent_path);
            char s[PATH_MAX + 40];
            snprintf(s, sizeof(s), "F: %s size: %llu (%s)\n", str, (unsigned long long)src_size, (src_size <= size_threshold ? "doesn't exceed threshold" : "exceeds threshold"));
            writeToLog(s);
            file_type dst_ft = get_file_type(dst_ent_path); // sprawdź typ elementu w katalogu docelowym
            switch (dst_ft)
            {
                case FT_REGULAR: // plik docelowy istnieje
                    {
                        time_t dst_mtime = get_mtime(dst_ent_path);
                        snprintf(str, sizeof(str), "File exists at the destination (%s)\n", (dst_mtime == src_mtime ? "same modification time" : "different modification time"));
                        writeToLog(str);
                        if (dst_mtime == src_mtime) break; //daty modyfikacji nie różnią się od siebie
                        copy_file(src_ent_path, dst_ent_path, src_size > size_threshold);
                    }
                    break;
                case FT_NONE: // plik docelowy nie istnieje
                    writeToLog("File doesn't exist at the destination\n");
                    copy_file(src_ent_path, dst_ent_path, src_size > size_threshold);
                    break;
                default: // element docelowy jest innego typu niż element źródłowy
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

    DIR *dir = opendir(path); // otwórz katalog do usunięcia
    if (dir == NULL) return -1;

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) // przeglądaj elementy w katalogu do usunięcia
    {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;

        char ent_path[PATH_MAX];
        snprintf(ent_path, sizeof(ent_path), "%s/%s", path, ent->d_name); // ścieżka elementu

        switch (ent->d_type) // sprawdź typ elementu
        {
            case DT_DIR: // katalog
                {
                    int res = remove_directory(ent_path);
                    if (res != 0)
                    {
                        closedir(dir);
                        return res;
                    }
                }
                break;
            case DT_REG: // zwykły plik
                snprintf(str, sizeof(str), "File to remove: %s\n", ent_path);
                writeToLog(str);
                if (remove(ent_path) == 0) writeToLog("Destination file removed\n");
                else
                {
                    closedir(dir);
                    return -3;
                }
                break;
            default: // inny typ pliku
                snprintf(str, sizeof(str), "Other type in directory to remove: %s\n", ent_path);
                writeToLog(str);
                closedir(dir);
                return -2;
        }
    }
    closedir(dir);
    if (remove(path) != 0) return -3;
    return 0;
}

void remove_extras(const char *src, const char *dst, bool recursive)
{
    DIR *dst_dir = opendir(dst); // otwórz katalog docelowy
    if (dst_dir == NULL)
    {
        char str[PATH_MAX + 30];
        snprintf(str, sizeof(str), "Failed opening directory (%s)\n", dst);
        writeToLog(str);
        return;
    }
    struct dirent *dst_ent;
    while ((dst_ent = readdir(dst_dir)) != NULL) // przeglądaj elementy w katalogu docelowym
    {
        if (strcmp(dst_ent->d_name, ".") == 0 || strcmp(dst_ent->d_name, "..") == 0) continue;

        char src_ent_path[PATH_MAX], dst_ent_path[PATH_MAX];
        snprintf(src_ent_path, sizeof(src_ent_path), "%s/%s", src, dst_ent->d_name); // ścieżka elementu źródłowego
        snprintf(dst_ent_path, sizeof(dst_ent_path), "%s/%s", dst, dst_ent->d_name); // ścieżka elementu docelowego

        time_t t = get_mtime(dst_ent_path);
        char ts[32];
        strftime(ts, sizeof(ts), "%Y/%m/%d %H:%M:%S", localtime(&t));
        char str[PATH_MAX + 40];
        snprintf(str, sizeof(str), "%s \"%s\"\n", ts, dst_ent_path);

        if (recursive && dst_ent->d_type == DT_DIR) // element docelowy jest katalogiem
        {
            char s[PATH_MAX + 40] = "D: ";
            strcat(s, str);
            writeToLog(s);
            file_type src_ft = get_file_type(src_ent_path); // sprawdź typ elementu w katalogu źródłowym
            switch (src_ft)
            {
                case FT_DIRECTORY: // podkatalog istnieje w katalogu źródłowym
                    writeToLog("Source directory exists\n");
                    remove_extras(src_ent_path, dst_ent_path, true);
                    break;
                case FT_NONE: // podkatalog nie istnieje w katalogu źródłowym
                    {
                        writeToLog("Source directory doesn't exist\n");
                        int res = remove_directory(dst_ent_path);
                        if (res == 0) snprintf(s, sizeof(s), "Directory removed (%s)\n", dst_ent_path);
                        else if (res == -1) snprintf(s, sizeof(s), "Failed removing directory (%s), couldn't open directory\n", dst_ent_path);
                        else if (res == -2) snprintf(s, sizeof(s), "Failed removing directory (%s), other file type exists in the directory\n", dst_ent_path);
                        else if (res == -3) snprintf(s, sizeof(s), "Failed removing directory (%s)\n", dst_ent_path);
                        writeToLog(s);
                    }
                    break;
                default: // element innego typu istnieje w katalogu źródłowym
                    writeToLog("Other type named like the destination directory exists at the source\n");
                    if (remove_directory(dst_ent_path) == 0) writeToLog("Destination directory removed\n");
                    else writeToLog("Failed to remove destination directory\n");
                    break;
            }
        }
        else if (dst_ent->d_type == DT_REG) // element docelowy jest zwykłym plikiem
        {
            char s[PATH_MAX + 40] = "F: ";
            strcat(s, str);
            writeToLog(s);
            file_type src_ft = get_file_type(src_ent_path); // sprawdź typ elementu w katalogu źródłowym
            switch (src_ft)
            {
                case FT_REGULAR: // plik źródłowy istnieje
                    writeToLog("Source file exists\n");
                    break;
                case FT_NONE: // plik źródłowy nie istnieje
                    writeToLog("Source file doesn't exist\n");
                    if (remove(dst_ent_path) == 0) writeToLog("Destination file removed\n");
                    else writeToLog("Failed to remove destination file\n");
                    break;
                default: // element innego typu istnieje w katalogu źródłowym
                    writeToLog("Other type named like the destination file exists at the source\n");
                    if (remove(dst_ent_path) == 0) writeToLog("Destination file removed\n");
                    else writeToLog("Failed to remove destination file\n");
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
