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
#include "filesync.h"

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define USE_SYSLOG 1

#if !USE_SYSLOG
int logfd;
#endif

void openLogFile()
{
    #if USE_SYSLOG
    openlog("filesyncd", LOG_PID, LOG_DAEMON);
    #else
    logfd = open("filesyncd.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
    #endif
}

void closeLogFile()
{
    #if USE_SYSLOG
    closelog();
    #else
    close(logfd);
    #endif
}

void writeToLog(char *str)
{
    #if USE_SYSLOG
    syslog(LOG_NOTICE, str);
    #else
    write(logfd, str, strlen(str));
    #endif
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
    #if USE_SYSLOG
    chdir("/");
    #endif

    /* Close all open file descriptors */
    int x;
    for (x = sysconf(_SC_OPEN_MAX); x>=0; x--)
    {
        close (x);
    }

    /* Open the log file */
    openLogFile();
}

#define print_usage() ( printf("Usage: %s source_path destination_path [-R] [-t sleep_time] [-s size_treshold]\n", argv[0]) )

int main(int argc, char* argv[])
{   
    if (argc < 3)
    {
        print_usage();
        return 0;
    }
    char *src, *dst;
    bool recursive = false;
    ssize_t size_treshold = 1000000;
    int sleep_time = 300;
    int i, op = 0;
    for (i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-')
        {
            if (argv[i][1] == 'R') recursive = true;
            else if (argv[i][1] == 't')
            {
                i++;
                int s = atoi(argv[i]);
                if (s > 0) sleep_time = s;
                else
                {
                    printf("Invalid sleep time!\n");
                    return 0;
                }
            }
            else if (argv[i][1] == 's')
            {
                i++;
                if (sscanf(argv[i], "%zu", &size_treshold) != 1)
                {
                    printf("Invalid size treshold!\n");
                    return 0;
                }
            }
            else
            {
                print_usage();
                return 0;
            }
        }
        else
        {
            op++;
            if (op == 1) src = argv[i];
            else if (op == 2) dst = argv[i];
        }
    }
    if (op != 2)
    {
        print_usage();
        return 0;
    }
    
    make_daemon();

    writeToLog("File Sync Daemon started.\n");

    while (1)
    {
        char str[256];
        snprintf(str, sizeof(str), "run_filesync(\"%s\", \"%s\", %s, %zu)\n", src, dst, recursive ? "true" : "false", size_treshold);
        writeToLog(str);
        run_filesync(src, dst, recursive, size_treshold);
        sleep(sleep_time);
    }

    writeToLog("File Sync Daemon terminated.\n");
    closelog();

    return EXIT_SUCCESS;
}
