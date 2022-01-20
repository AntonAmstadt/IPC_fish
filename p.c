#include <stdio.h> 
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdlib.h>
#include <unistd.h> 
#include <sys/wait.h>
#include <signal.h>
#include <time.h>

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

void handleSigusr2(int sig) {
    sigEnd = 1;
}


int main() {
    time_t t;
    const int MAX_PROC = 19;
    pid_t processIds[MAX_PROC];
    int sigCount[MAX_PROC];
    struct sigaction action;
    action.sa_handler = handleSigusr1;
    sigemptyset(&action.sa_mask);
    sigaddset(&action.sa_mask, SIGUSR1);
    sigaddset(&action.sa_mask, SIGUSR2);
    action.sa_flags = 0;

    for (int i = 0; i < MAX_PROC; i++){
        processIds[i] = -1;
        sigCount[i] = 0;
    }

    // initialize random number generator for pellet drops
    srand((unsigned) time(&t));

    printf("parent process id is:%i\n", getpid());

    pid_t fishPid, pp; // process id of fish
    sigset_t set, emptyset; // will use this with sigsuspend to wait for signals

    // set up signal handler
    sigaction(SIGUSR1, &action, NULL);
    sigaction(SIGUSR2, &action, NULL);
    
    // make empty signal set
    sigemptyset(&emptyset);
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    sigaddset(&set, SIGUSR2);


    // arguments for execve; empty because I am not passing anything to the fish or pellet processes
    char* args[] = {NULL};
    char* env[] = {NULL};

    sigprocmask(SIG_BLOCK, &set, NULL);

    // run the fish process
    switch(processIds[0] = fork()) {
        case -1:
            printf("error in fork\n");
            exit(1);
        case 0:
            execve("./c", args, env);
            printf("error in fish execve\n"); // execve should not return
            exit(1); 
        default:
            printf("c.c forked with process id %i\n", processIds[0]); 
            //pid = wait(NULL); // continue in parent
    }

    switch(processIds[1] = fork()) {
        case -1:
            printf("error in fork\n");
            exit(1);
        case 0:
            execve("./pp", args, env);
            printf("error in fish execve\n"); // execve should not return
            exit(1); 
        default:
            printf("pp.c forked with process id %i\n", processIds[1]); 
            //pid = wait(NULL); // continue in parent
    }

    //testing start
    //printf("in swim_mill, fish has been forked\n");

    int i = 0;
    
    sigCont = 1;
    sigEnd = 0;
    int k = 0;

    while (sigCont && !sigEnd) {
        sigCont = 0;
        // send SIGUSR1 signal to fish

        if ((rand()%10) == 0) {
            int found = 0;
            int proc = 0;
            while ((found == 0) && proc < MAX_PROC) {
                if (processIds[proc] == -1){
                    found = 1;
                    switch(processIds[proc] = fork()) {
                        case -1:
                            printf("error in fork\n");
                            exit(1);
                        case 0:
                            execve("./pp", args, env);
                            printf("error in fish execve\n"); // execve should not return
                            exit(1); 
                        default:
                            printf("pp.c forked with process id %i\n", processIds[proc]); 
                            sigCount[proc] = 0;
                            //pid = wait(NULL); // continue in parent
                    }
                }
                proc++;
            }
            
        }


        while(processIds[i % MAX_PROC] == -1) {
            i = (i + 1) % MAX_PROC;
        }
        
        if(i%MAX_PROC == 0){
            kill(processIds[0], SIGUSR1);
        }
        else if(++sigCount[i%MAX_PROC] < 12) {
            kill(processIds[i%MAX_PROC], SIGUSR1);
        }
        else {
            kill(processIds[i%MAX_PROC], SIGUSR2);
            processIds[i%MAX_PROC] = -1;
        }
        
        //kill(processIds[1], SIGUSR1);
        // wait for any signal
        printf("in parent, before sigsuspend, k = %i\n", k);
        sigsuspend(&emptyset);
        i++;
        printf("in parent, after sigsuspend, k = %i\n", k);
        if (k >= 200000) {
            for (int j = MAX_PROC; j >= 0; j--){
                if (processIds[j] != -1) { 
                    kill(processIds[j], SIGUSR2);
                    wait(NULL);
                }
            }
        }
        k++;

    }
    //kill(fishPid, SIGUSR2);

    //testing end

    printf("parent end\n");
    
return 0;
}