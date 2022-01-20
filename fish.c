#include <stdio.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>

static volatile sig_atomic_t sigCont;
static volatile sig_atomic_t sigEnd;
static volatile sig_atomic_t alarmFlag;
static volatile sig_atomic_t intFlag;

void handleSigusr1(int sig) {
    if (sig == SIGUSR1) {
        sigCont = 1;
    }

    if (sig == SIGUSR2) {
        sigEnd = 1; 
    }
}

void handleAlarmAndSigint(int sig) {
    sigset_t endingset;
    // this set will be used to block signals after alarm or ctrl c has been triggered
    sigfillset(&endingset);
    // prevent multiple ctrl c, or alarm after ctrl c
    sigprocmask(SIG_BLOCK, &endingset, NULL);
    
    //set flow of control flag based on signal received
    if (sig == SIGALRM) {
        alarmFlag = 1;
    }
    if (sig == SIGINT) {
        intFlag = 1;
    }
}

int main(int argc, char* argv[]) {
    alarmFlag = 0;
    intFlag = 0;
    printf("fish process id is:%i\n", getpid());


    sigset_t set, emptyset; // will use this with sigsuspend to wait for signals
    
    // this next chunk of code is the same as swim_mill
    // see swim_mill for the comments
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

    struct sigaction endingaction;
    endingaction.sa_handler = handleAlarmAndSigint;
    sigemptyset(&endingaction.sa_mask);
    sigaddset(&endingaction.sa_mask, SIGUSR1);
    sigaddset(&endingaction.sa_mask, SIGUSR2);
    sigaddset(&endingaction.sa_mask, SIGINT);
    sigaddset(&endingaction.sa_mask, SIGALRM);
    endingaction.sa_flags = 0;
     
    sigaction(SIGINT, &endingaction, NULL);
    sigaction(SIGALRM, &endingaction, NULL);

    int i = 0;
    sigprocmask(SIG_BLOCK, &set, NULL);

    // recreate a key for the shared memory
    key_t shmKey = ftok("swim_mill.c", 1);

    // get read/write access to the shared memory segment created by swim_mill
    int smId = shmget(shmKey, 100 * sizeof(char), 0666);

    // attach fish to the shared memory segment
    char *smPtr = (char *)shmat(smId, NULL, 0);


    // location of the fish
    int location = 95;
    smPtr[location] = 'F';

    // flow of execution flags
    sigCont = 1;
    sigEnd = 0;

    while (sigCont && !sigEnd) {
        sigCont = 0;
        //debug statment below
        //printf("fish: before sigsuspend\n");

        // wait for a signal
        sigsuspend(&emptyset);
        
        // debug statement below
        //printf("fish: after sigsuspend\n");

        if(intFlag){
            fprintf(stderr,"fish.c with pid: %i terminated due to ctrl + c\n", getpid());
            int detach; // return value of shmdt

            // detach from shared memory segment
            detach = shmdt((void *) smPtr);
            if (detach < 0){
                printf("error on shmdt\n");
            }
            exit(1);
        }
        if (alarmFlag) {
            printf("fish pid: %i terminated due to alarm in location: %i\n", getpid(), location);
            int detach; // return value of shmdt

            // detach from shared memory segment
            detach = shmdt((void *) smPtr);
            if (detach < 0){
                printf("error on shmdt\n");
            }
            return 0;
        }
        if(!sigEnd){
            // smPtr[location] = '0';
            // smPtr[--location] = 'F';

            // fish movement start
            // first check spaces directly to the left and right of fish for pellets
            if (location - 1 >= 90 && smPtr[location - 1] == '1') {
                smPtr[location] = '0';
                location--;
                smPtr[location] = 'F';
            }
            else if (location + 1 <= 99 && smPtr[location + 1] == '1') {
                smPtr[location] = '0';
                location++;
                smPtr[location] = 'F';
            }
            else {
                int found = 0;
                int row = 8;
                // search for the closest pellet in the rows above fish starting from closest rows
                while (!found && row >= 0){
                    if (smPtr[(row * 10) + (location % 10)] == '1') {// pellet directly above fish -> don't move
                        found = 1;
                    }
                    else {
                        int l = (location % 10) - 1; // for checking spaces to the left of fish
                        int r = (location % 10) + 1; // for checking spaces to the right of fish
                        while (l >= 0 || r <= 9) { // check row for closest pellet
                            if (l >= 0 && smPtr[(row * 10) + l] == '1'){
                                found = 1;
                                smPtr[location - 1] = 'F';
                                smPtr[location] = '0';
                                location--;
                            }
                            else if (r <= 9 && smPtr[(row * 10) + r] == '1'){
                                found = 1;
                                smPtr[location + 1] = 'F';
                                smPtr[location] = '0';
                                location++;
                            }


                            l--;
                            r++;
                        }
                    }
                    row--;
                }
                //no pellets on map, go to center
                if (!found) {
                    if (location < 95) {
                        smPtr[location + 1] = 'F';
                        smPtr[location] = '0';
                        location++;
                    }
                    else if (location > 95) {
                        smPtr[location - 1] = 'F';
                        smPtr[location] = '0';
                        location--;
                    }
                }
            }
            // fish movement end

            // for(int row = 0; row < 10; row++){
            //     for(int col = 0; col < 10; col++){
            //         printf("%c ", smPtr[(row*10)+col]);
            //     }
            //     printf("\nfish\n");
            // }
            // send SIGUSR1 signal to swim_mill
            kill(getppid(), SIGUSR1);
        }
    }


    // printf("in fish\n");
    
    // smPtr[95] = 'F';
    // for(int row = 0; row < 10; row++){
    //     for(int col = 0; col < 10; col++){
    //         printf("%c ", smPtr[(row*10)+col]);
    //     }
    //     printf("\n");
    // }
    // printf("end fish\n");

    int detach; // return value of shmdt

    // detach from shared memory segment
    detach = shmdt((void *) smPtr);
    if (detach < 0){
        printf("error on shmdt\n");
    }
    printf("fish detached, sending sigusr2 to swim_mill\n");
    kill(getppid(), SIGUSR2);

    return 0;
}