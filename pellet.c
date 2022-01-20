#include <stdio.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>

//A NOTE ON PELLET LOCATIONS
/*  You may see that when the program terminates, a pellet may have a 
    location >= 100. This is not a location inside the swim_mill map.
    Instead this is a dummy location which means the pellet is waiting
    to be terminated by the swim_mill. This is intended.
*/

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
    
    // set flow of control flag based on signal received
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

    printf("new pellet created with process id is:%i\n", getpid());
    srand(time(0));


    sigset_t set, emptyset; // will use this with sigsuspend to wait for signals
    
    // make empty signal set 
    sigemptyset(&emptyset);
    sigemptyset(&set);

    // add these signals to set
    sigaddset(&set, SIGUSR1);
    sigaddset(&set, SIGUSR2);

    // action is for a response to a SIGUSR1 or SIGUSR2 signal
    struct sigaction action;
    action.sa_handler = handleSigusr1;
    sigemptyset(&action.sa_mask);
    sigaddset(&action.sa_mask, SIGUSR1);
    sigaddset(&action.sa_mask, SIGUSR2);
    action.sa_flags = 0;
    
    sigaction(SIGUSR1, &action, NULL);
    sigaction(SIGUSR2, &action, NULL);

    // endingaction is for a response to a SIGINT or SIGALRM signal
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

    sigprocmask(SIG_BLOCK, &set, NULL);

    // recreate a key for the shared memory
    key_t shmKey = ftok("swim_mill.c", 1);

    // get read/write access to the shared memory segment created by swim_mill
    int smId = shmget(shmKey, 100 * sizeof(char), 0666);

    // attach fish to the shared memory segment
    char *smPtr = (char *)shmat(smId, NULL, 0);

    int valid = 0;
    //random location
    int location = (rand() + (getpid() % 37)) % 90;
    // place the pellet in a random empty location in a row above fish
    while(!valid){
        if (smPtr[location] == '0') {
            valid = 1;
            smPtr[location] = '1';
        }
        else { // pick an adjacent location
            location--;
            if (location < 0) {
                location = 89;
            }
        }
    }

    printf("pellet with process id:%i set it's starting location to %i\n", getpid(), location);
    

    // make sure flags are set to defaults before main loop
    sigCont = 1;
    sigEnd = 0;
    int eaten = 0;

    while (sigCont && !eaten && location <= 99) {
        // debug statment below
        //printf("pellet: %i before sigsuspend\n", getpid());

        // wait for anysignal
        sigsuspend(&emptyset);

        // debug statment below
        //printf("pellet: %i after sigsuspend\n", getpid());

        // case of SIGINT received
        if(intFlag){
            fprintf(stderr,"pellet with pid: %i terminated due to ctrl + c\n", getpid());
            int detach; // return value of shmdt

            // detach from shared memory segment
            detach = shmdt((void *) smPtr);
            if (detach < 0){
                printf("error on shmdt\n");
            }
            exit(1);
        }
        // case of SIGALRM received
        if (alarmFlag) {
            printf("pellet pid: %i terminated due to alarm in location: %i\n", getpid(), location);
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

            // pellet movement start
            if (smPtr[location] == 'F') {
                eaten = 1;
                printf("pellet pid: %i was eaten in location: %i\n", getpid(), location);
            }
            else if (location + 10 <= 99 && smPtr[location + 10] == 'F') {
                eaten = 1;
                printf("pellet pid: %i was eaten in location: %i\n", getpid(), location + 10);
                smPtr[location] = '0';
                location += 10;
            }
            else if (location + 10 <= 99) {
                smPtr[location] = '0';
                location += 10;
                smPtr[location] = '1';
            }
            else {
                printf("pellet pid: %i was missed in location: %i\n", getpid(), location);
                smPtr[location] = '0';
                location += 10;
            }
            // pellet movement end
            
            // debug statement below
            //printf("pellet pid: %i has location: %i\n", getpid(), location);

            // send SIGUSR1 signal to swim_mill
            kill(getppid(), SIGUSR1);
        }
        else {
            
            int detach; // return value of shmdt

            // detach from shared memory segment
            detach = shmdt((void *) smPtr);
            if (detach < 0){
                printf("error on shmdt\n");
            }
            kill(getppid(), SIGUSR1);
            // OUTPUT THE ENDING????
            return 0;

        }
    }

    while (!sigEnd){
        // debug statment below
        //printf("pellet: %i waiting to be killed\n", getpid());

        // make swim_mill continue execution
        kill(getppid(), SIGUSR1);

        // wait for any signal
        sigsuspend(&emptyset);


        // debug statment below
        //printf("in pellet %i, before sigsuspend part 2\n", getpid());

        // ctrl c was pressed
        if(intFlag){
            fprintf(stderr,"pellet with pid: %i terminated due to ctrl + c\n", getpid());
            int detach; // return value of shmdt

            // detach from shared memory segment
            detach = shmdt((void *) smPtr);
            if (detach < 0){
                printf("error on shmdt\n");
            }
            exit(1);
        }

        // alarm ran out of time
        if (alarmFlag) {
            printf("pellet pid: %i terminated due to alarm in location: %i\n", getpid(), location);
            int detach; // return value of shmdt

            // detach from shared memory segment
            detach = shmdt((void *) smPtr);
            if (detach < 0){
                printf("error on shmdt\n");
            }
            return 0;
        }

        // sigCount got too high
        if (sigEnd){
            //debug statment below
            //printf("pellet: %i just died2\n", getpid());

            // make swim_mill continue
            kill(getppid(), SIGUSR1);
            
            int detach; // return value of shmdt

            // detach from shared memory segment
            detach = shmdt((void *) smPtr);
            if (detach < 0){
                printf("error on shmdt\n");
            }
            return 0;
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

    return 0;
}