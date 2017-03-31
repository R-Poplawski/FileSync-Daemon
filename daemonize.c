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

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define USE_SYSLOG 1

#if !USE_SYSLOG
int logfd;
#endif

void openLogFile()
{
    #if USE_SYSLOG
    openlog("firstdaemon", LOG_PID, LOG_DAEMON);
    #else
    logfd = open("filesyncdaemon.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
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

static void skeleton_daemon()
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
	    //wait(NULL);
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

int main(int argc, char* argv[])
{   
    if (argc < 3 || argc > 4)
    {
        printf("Usage: %s source_path destination_path [-R]\n", argv[0]);
        return 0;
    }
    skeleton_daemon();

    writeToLog("First daemon started.\n");
    char i = '1';

    char str[7] = "i = 0\n";

    while (1)
    {
        //TODO: Insert daemon code here.

        sleep(5);
        str[4] = i;
        writeToLog(str);
        i++;
        if (i > '5') break;
    }

    writeToLog("First daemon terminated.\n");
    closelog();

    return EXIT_SUCCESS;
}
