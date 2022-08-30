#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#define MAXLINE 8192
#define MAXARGS 100
#define MAXPIPE 10

void unix_error(char *msg) /* Unix-style error */
{
    fprintf(stderr, "%s: %s\n", msg, strerror(errno));
    exit(0);
}

pid_t Fork(void) /* fork wrapper */
{
    pid_t pid;

    if((pid = fork()) < 0)
        unix_error("Fork error\n");
    return pid;
}

/* execve wrapper */
void Execve(char *argv_0, char *const argv[], char *const envp[])
{
    char filename[MAXLINE];
    int fail;

    strcpy(filename, "/bin/");
    strcat(filename, argv_0);

    fail = execve(filename, argv, envp);
    if(fail < 0){	// /bin에 존재하지 않은 명령어 (sort 등)
        strcpy(filename, "/usr/bin/");
        strcat(filename, argv_0);

        fail = execve(filename, argv, envp);
    }
    
    if(fail < 0)
	unix_error("Execve error");
}

/* waitpid wrapper */
pid_t Waitpid(pid_t pid, int *status, int option)
{
    pid_t ret;
    if((ret = waitpid(pid, status, option)) >= 0)
    	return ret;
}

typedef void handler_t(int);

/* sigprocmask wrapper */
void Sigprocmask(int how, const sigset_t *set, sigset_t *oldset)
{
    if(sigprocmask(how, set, oldset) < 0)
        unix_error("Sigprocmask error");
    return;
}

void Sigemptyset(sigset_t *set)
{
    if(sigemptyset(set) < 0)
        unix_error("Sigemptyset error");
    return;
}

void Sigfillset(sigset_t *set)
{
    if (sigfillset(set) < 0)
        unix_error("Sigfillset error");
    return;
}

void Sigaddset(sigset_t *set, int signum)
{
    if (sigaddset(set, signum) < 0)
        unix_error("Sigaddset error");
    return;
}

void Sigdelset(sigset_t *set, int signum)
{
    if (sigdelset(set, signum) < 0)
        unix_error("Sigdelset error");
    return;
}

int Sigsuspend(const sigset_t *set){
    int rc = sigsuspend(set);
    if(errno != EINTR)
        unix_error("Sigsuspend error");
    return rc;
}

/* sigaction */
handler_t *Signal(int signum, handler_t *handler)
{
    struct sigaction action, old_action;

    action.sa_handler = handler;
    Sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART;

    if(sigaction(signum, &action, &old_action) < 0)
        unix_error("Signal error");
    return (old_action.sa_handler);
}

void Kill(pid_t pid, int signum)
{
    int rc;
    if((rc == kill(pid, signum)) < 0)
        unix_error("Kill error");
}                                                                    
