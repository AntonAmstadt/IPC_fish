#include <stdio.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>

static volatile sig_atomic_t sigCont;
static volatile sig_atomic_t sigEnd;

void handleSigusr1(int sig) {
    if (sig == SIGUSR1) {
        sigCont = 1;
    }

    if (sig == SIGUSR2) {
        sigEnd = 1; 
    }
}


int main(int argc, char* argv[]) {
    printf("child process id is:%i\n", getpid());


    sigset_t set, emptyset; // will use this with sigsuspend to wait for signals
    
    // make empty signal set 
    sigemptyset(&emptyset);
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    sigaddset(&set, SIGUSR2);

    struct sigaction action;
    action.sa_handler = handleSigusr1;
    sigemptyset(&action.sa_mask);
    sigaddset(&action.sa_mask, SIGUSR1);
    sigaddset(&action.sa_mask, SIGUSR2);
    action.sa_flags = 0;
    
    sigaction(SIGUSR1, &action, NULL);
    sigaction(SIGUSR2, &action, NULL);

    int i = 0;
    sigprocmask(SIG_BLOCK, &set, NULL);
    sigCont = 1;

    while (sigCont) {
        sigCont = 0;
        printf("about to send SIGUSR1 to parent from c\n");
        kill(getppid(), SIGUSR1);
        // wait for any signal
        printf("in child, before sigsuspend, j = %i\n", i);
        sigsuspend(&emptyset);
        i++;
        printf("in child, after sigsuspend, j = %i\n", i);

    }
    kill(getppid(), SIGUSR2);

    printf("done in child\n");

    return 0;
}