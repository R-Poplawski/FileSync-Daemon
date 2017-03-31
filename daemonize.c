/*
 * daemonize.c
 * This example daemonizes a process, writes a few log messages,
 * sleeps 20 seconds and terminates afterwards.
 */

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

int logfd;

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
    //chdir("/");

    /* Close all open file descriptors */
    int x;
    for (x = sysconf(_SC_OPEN_MAX); x>=0; x--)
    {
        close (x);
    }

    /* Open the log file */
    //openlog ("firstdaemon", LOG_PID, LOG_DAEMON);
}

void openLogFile()
{
    logfd = open("mydaemon_log", O_WRONLY | O_APPEND, 0644);
}

void writeToLog(char *str)
{
    write(logfd, str, strlen(str));
}

int main()
{    
    skeleton_daemon();
    openLogFile();

    writeToLog("First daemon started.\n");
    char i = '1';

    char str[7] = "i = 0\n";

    while (1)
    {
        //TODO: Insert daemon code here.
        //syslog (LOG_NOTICE, "First daemon started.");
        
        sleep(5);
	str[4] = i;
	writeToLog(str);
	printf(str);
        i++;
        if (i > '5') break;
    }

    /*syslog (LOG_NOTICE, "First daemon terminated.");
    closelog();*/
    close(logfd);

    return EXIT_SUCCESS;
}
