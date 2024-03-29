#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include <fcntl.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <time.h>
#include <limits.h>
#include "filesync.h"

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

bool uselog = false;

void openLogFile()
{
    openlog("filesyncd", LOG_PID, LOG_DAEMON);
    uselog = true;
}

void closeLogFile()
{
    closelog();
    uselog = false;
}

void writeToLog(const char *str)
{
    if (uselog) syslog(LOG_NOTICE, str);
    else printf(str);
}

void handle_SIGUSR1(int signum)
{
    if (signum == SIGUSR1)
    {
        writeToLog("SIGUSR1\n");
    }
}

static void make_daemon()
{
    pid_t pid;

    /* Fork off the parent process */
    pid = fork();

    /* An error occurred */
    if (pid < 0)
        exit(EXIT_FAILURE);

    /* Success: Let the parent terminate */
    if (pid > 0)
    {
        //wait(NULL);
        exit(EXIT_SUCCESS);
    }

    /* On success: The child process becomes session leader */
    if (setsid() < 0)
        exit(EXIT_FAILURE);

    /* Catch, ignore and handle signals */
    //TODO: Implement a working signal handler */
    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);
    //signal(SIGUSR1, handle_SIGUSR1);
    struct sigaction sa;
    sa.sa_handler = handle_SIGUSR1;
    sigaction(SIGUSR1, &sa, NULL);

    /* Fork off for the second time*/
    pid = fork();

    /* An error occurred */
    if (pid < 0)
        exit(EXIT_FAILURE);

    /* Success: Let the parent terminate */
    if (pid > 0)
    {
        wait(NULL);
        exit(EXIT_SUCCESS);
    }

    /* Set new file permissions */
    umask(0);

    /* Change the working directory to the root directory */
    /* or another appropriated directory */
    /*
    chdir("/");
    */

    /* Close all open file descriptors */
    int x;
    for (x = sysconf(_SC_OPEN_MAX); x >= 0; x--)
    {
        close (x);
    }

    /* Open the log file */
    openLogFile();
}

#define print_usage() ( printf("Usage: %s source_path destination_path [-OPTIONS]\n"\
                                "OPTIONS:\n"\
                                "-R\t\t\tRecursive synchronization (include subdirectories)\n"\
                                "-t sleep_time\t\tSets number of seconds between synchronizations\n"\
                                "-s size_threshold\tSets file size threshold at which mmap will be used\n"\
                                "-S\t\t\tSingle synchronization\n", argv[0]) )

int main(int argc, char *argv[])
{   
    if (argc < 3)
    {
        print_usage();
        return 0;
    }
    char *src, *dst;
    bool recursive = false, single = false;
    off_t size_threshold = 1000000;
    int sleep_time = 300;
    int i, op = 0;
    for (i = 1; i < argc; i++) // dla każdego argumentu programu
    {
        if (argv[i][0] != '-') // sprawdź czy argument zaczyna się od '-'
        {
            op++;
            if (op == 1) src = argv[i];
            else if (op == 2) dst = argv[i];
            continue;
        }
        if (strlen(argv[i]) != 2)
        {
            print_usage();
            return 0;
        }
        switch (argv[i][1]) // sprawdź argument opcjonalny
        {
            case 'R': // wywołanie rekurencyjne
                recursive = true;
                break;
            case 't': // czas między wywołaniami
                i++;
                int s = atoi(argv[i]);
                if (s > 0) sleep_time = s;
                else
                {
                    printf("Invalid sleep time!\n");
                    return 0;
                }
                break;
            case 's': // próg rozmiaru
                i++;
                if (sscanf(argv[i], "%zu", &size_threshold) != 1)
                {
                    printf("Invalid size threshold!\n");
                    return 0;
                }
                break;
            case 'S': // pojedyncza synchronizacja
                single = true;
                break;
            default:
                print_usage();
                return 0;
        }
    }
    if (op != 2)
    {
        print_usage();
        return 0;
    }
    
    // pełne ścieżki katalogów
    char real_src[PATH_MAX];
    char real_dst[PATH_MAX];
    realpath(src, real_src);
    realpath(dst, real_dst);

    if (get_file_type(real_src) != FT_DIRECTORY) // sprawdź czy istnieje katalog źródłowy
    {
        printf("Invalid source directory!\n");
        return 0;
    }
    if (get_file_type(real_dst) != FT_DIRECTORY) // sprawdź czy istnieje katalog docelowy
    {
        printf("Invalid destination directory!\n");
        return 0;
    }
    if (strcmp(real_src, real_dst) == 0) // sprawdź czy katalogi są różne
    {
        printf("The destination directory must be different from the source directory!\n");
        return 0;
    }
    
    if (recursive) // jeśli wybrano tryb rekurencyjny, sprawdź czy katalogi nie zawierają się w sobie
    {
        if (path_contains(real_src, real_dst))
        {
            printf("Source directory must not contain the destination directory!\n");
            return 0;
        }
        else if (path_contains(real_dst, real_src))
        {
            printf("Destination directory must not contain the source directory!\n");
            return 0;
        }
    }
    
    if (single) // pojedyncza synchronizacja
    {
        run_filesync(real_src, real_dst, recursive, size_threshold);
        return 0;
    }

    make_daemon();

    writeToLog("File Sync Daemon started\n");

    while (1)
    {
        run_filesync(real_src, real_dst, recursive, size_threshold);
        sleep(sleep_time);
    }

    writeToLog("File Sync Daemon terminated\n");
    closelog();

    return EXIT_SUCCESS;
}
