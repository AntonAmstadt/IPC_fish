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
    printf("pp process id is:%i\n", getpid());


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
    sigEnd = 0;

    // while (sigCont) {
    //     sigCont = 0;
        
    //     printf("about to send SIGUSR1 to parent from pp\n");
    //     kill(getppid(), SIGUSR1);
    //     // wait for any signal
    //     printf("in pp, before sigsuspend, x = %i\n", i);
    //     sigsuspend(&emptyset);
    //     i++;
    //     printf("in pp, after sigsuspend, x = %i\n", i);

    // }

    // s new
    while (sigCont && (i < 10)) {
        sigCont = 0;
        
        printf("about to send SIGUSR1 to parent from pp\n");
        kill(getppid(), SIGUSR1);
        // wait for any signal
        printf("in pp, before sigsuspend, x = %i\n", i);
        sigsuspend(&emptyset);
        i++;
        printf("in pp: %i, after sigsuspend, x = %i\n", getpid(), i);

        if (sigEnd){
            printf("pp: %i just died1\n", getpid());
            kill(getppid(), SIGUSR1); 
            return 0;
        }

    }

    
    while (1){
        printf("pp: %i waiting to be killed\n", getpid());
        kill(getppid(), SIGUSR1);
        // wait for any signal
        printf("in pp, before sigsuspend part 2\n");
        sigsuspend(&emptyset);
        if (sigEnd){
            printf("pp: %i just died2\n", getpid());
            kill(getppid(), SIGUSR1);
            return 0;
        }

    }
    // end new

    printf("done in pp %i\n", getpid());

    return 0;
}