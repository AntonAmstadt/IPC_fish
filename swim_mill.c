#include <stdio.h> 
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdlib.h>
#include <unistd.h> 
#include <sys/wait.h>
#include <signal.h>
//#include <time.h>
#include <libexplain/fork.h>

static volatile sig_atomic_t sigCont;
static volatile sig_atomic_t sigEnd;
static volatile sig_atomic_t alarmFlag;
static volatile sig_atomic_t intFlag;

void handleSigusr1(int sig) {
    // set flow of control flag based on signal received
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

int main() {
    // output file has the map printed to it periodically
    FILE * outFile;
    outFile = fopen("output.txt", "w+");

    //control flags
    alarmFlag = 0;
    intFlag = 0;
    printf("swim_mill process id is:%i\n", getpid());

    int smId; // shared memory key
    char* smPtr; // shared memory pointer
    int detach; // return value of shmdt
    int ctl; // return value of shmctl

    // we have a max of 19 child processes since we can't have more than 20 processes
    const int MAX_PROC = 1000;

    // this array holds the pid for all the current child processes
    pid_t processIds[MAX_PROC];

    // sigCount is incremented every time swim_mill sends a signal to a child process
    // the index in processIds and in sigCount corresponds to the same child
    // the signal count is how I know that a pellet is for sure done descending the map
    // and has been eaten or missed
    int sigCount[MAX_PROC];

    // action is the sigaction struction that is necessary for sigaction()
    // action is used when receiving a SIGUSR1 or SIGUSR2 signal
    struct sigaction action;
    action.sa_handler = handleSigusr1;
    sigemptyset(&action.sa_mask);
    sigaddset(&action.sa_mask, SIGUSR1);
    sigaddset(&action.sa_mask, SIGUSR2);
    action.sa_flags = 0;

    // endingaction is the sigaction struction that is necessary for sigaction()
    // endingaction is used when receiving a SIGINT or SIGALRM signal
    struct sigaction endingaction;
    endingaction.sa_handler = handleAlarmAndSigint;
    sigemptyset(&endingaction.sa_mask);
    sigaddset(&endingaction.sa_mask, SIGUSR1);
    sigaddset(&endingaction.sa_mask, SIGUSR2);
    sigaddset(&endingaction.sa_mask, SIGINT);
    sigaddset(&endingaction.sa_mask, SIGALRM);
    endingaction.sa_flags = 0;

    // initialize the parallel child process arrays
    // when processIds[x] == -1, that means there is no process at that index
    for (int i = 0; i < MAX_PROC; i++){
        processIds[i] = -1;
        sigCount[i] = 0;
    }

    // initialize random number generator for pellet drops
    srand(time(0));

    // the signal sets that will be used
    sigset_t set, emptyset, endingset;

    // set up signal handlers

    // these signals get handled by handleSigusr1() (bad name, but I made it early in the project)
    sigaction(SIGUSR1, &action, NULL);
    sigaction(SIGUSR2, &action, NULL);

    // these signals get handled by handleAlarmAndSigint() (much better name)
    sigaction(SIGINT, &endingaction, NULL);
    sigaction(SIGALRM, &endingaction, NULL);

    // start alarm
    alarm(30); // set to 10 if you want this program to work
    //alarm(10);
    
    // make empty signal set
    sigemptyset(&emptyset);

    // signal set for sigprocmask()
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    sigaddset(&set, SIGUSR2);

    

    // create a key for the shared memory
    key_t shmKey = ftok("swim_mill.c", 1);

    // create shared memory segment with enough space for 10 x 10 array
    smId = shmget(shmKey, 100 * sizeof(char), IPC_CREAT | 0666); 

    // smId is -1 on shmget error
    if (smId < 0) {
        printf("shmget error\n");
        exit(1);
    }

    // attach the shared memory segment to the virtual address space of swim_mill.c
    smPtr = (char *) shmat(smId, NULL, 0);


    // initialize all elements of the map to '0'
    // '0' represents a location with nothing in it
    // '1' represents a location with a pellet
    // 'F' represents a location with the fish
    // elements 0-9 represent the top level of the map, 10-19 is the next level down, ... , 90-99 is the bottom level with the fish
    for(int i = 0; i < 100; i++){
        smPtr[i] = '0';
    }
    
    // debugging code below
    // // outputs the map
    // for(int row = 0; row < 10; row++){
    //     for(int col = 0; col < 10; col++){
    //         printf("%c ", smPtr[(row*10)+col]);
    //     }
    //     printf("\n");
    // }
    // //

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
            execve("./fish", args, env);
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
            execve("./pellet", args, env);
            printf("error in pellet execve\n"); // execve should not return
            exit(1); 
        default:
            printf("pp.c forked with process id %i\n", processIds[1]); 
            //pid = wait(NULL); // continue in parent
    }

    // i used for cycling through child process arrays
    int i = 0;
    
    // flags for flow of control set to default
    sigCont = 1;
    sigEnd = 0;

    // k used for periodic output of map to output.txt
    int k = 0;

    //main loop of execution
    //probably equivalent to use -- while(sigCont) -- but I don't want to mess with it
    while (sigCont && !sigEnd) {
        // reset sigCont each loop
        // sigCont is set to 1 when SIGUSR1 is received
        sigCont = 0;
        
        // random generation of pellets: 10% chance each time the swim mill goes
        // through this main while loop
        if ((rand()%10) == 0 && !intFlag && !alarmFlag) {
            int found = 0;
            int proc = 0;
            // if pellet is supposed to be generated, go through the child process array and 
            // find empty spot for new process (if no space, then child not created)
            while ((found == 0) && proc < MAX_PROC) {
                if (processIds[proc] == -1){
                    found = 1;
                    switch(processIds[proc] = fork()) {
                        case -1: ;
                            char message[3000];
                            explain_message_fork(message, sizeof(message));
                            fprintf(stderr, "%s\n", message);
                            exit(1);
                        case 0:
                            execve("./pellet", args, env);
                            printf("error in pellet execve\n"); // execve should not return
                            exit(1); 
                        default:
                            printf("pellet forked with process id %i\n", processIds[proc]); 
                            // reset the signal count since the new process has been signaled 0 times at this point
                            sigCount[proc] = 0;
                    }
                }
                proc++;
            }
            
        }

        // find the next child process to send a signal to
        // fish is always processIds[0] and has a non -1 pid
        while(processIds[i % MAX_PROC] == -1) {
            i = (i + 1) % MAX_PROC;
        }

        // if I'm sending a signal to fish, don't mess with sigCount[]
        if(i%MAX_PROC == 0){
            kill(processIds[0], SIGUSR1);
        }

        // else if I'm sending a signal to a pellet that has been signaled < 10 times,
        // then send SIGUSR1 and increment the corresponding sigCount
        else if(++sigCount[i%MAX_PROC] < 11) {
            kill(processIds[i%MAX_PROC], SIGUSR1);
        }

        // I'm signalling a pellet that is ready to die, send it SIGUSR2 and open the slot in processIds[]
        else {
            kill(processIds[i%MAX_PROC], SIGUSR2);
            processIds[i%MAX_PROC] = -1;
        }

        // commented out debugging statment on line below
        // printf("in parent, before sigsuspend, k = %i\n", k);

        // wait for any signal
        sigsuspend(&emptyset);
        // signal mask restored now

        // commented out debugging statment on line below
        // printf("in parent, after sigsuspend, k = %i\n", k);

        if (intFlag) { // ctrl c has been entered by user
            // send SIGINT to all children so they can terminate correctly
            for (int j = MAX_PROC; j >= 0; j--){
                if (processIds[j] != -1) { 
                    kill(processIds[j], SIGINT);
                    wait(NULL);
                }
            }
            // detach from shared memory segment
            detach = shmdt((void *) smPtr);
            if (detach < 0){
                printf("error on shmdt\n");
            }

            // delete the shared memory segment
            ctl = shmctl(smId, IPC_RMID, NULL);
            if (ctl < 0){
                printf("error on shmctl\n");
            }
            fprintf(stderr, "In swim_mill, all children have been killed and shared memory has been destroyed (hopefully)\n");
            fclose(outFile);
            return 0;
        }

        if (alarmFlag) { // alarm has finished
            // send SIGALRM to all children so they can terminate correctly
            for (int j = MAX_PROC; j >= 0; j--){
                if (processIds[j] != -1) { 
                    kill(processIds[j], SIGALRM);
                    wait(NULL);
                }
            }
            // detach from shared memory segment
            detach = shmdt((void *) smPtr);
            if (detach < 0){
                printf("error on shmdt\n");
            }

            // delete the shared memory segment
            ctl = shmctl(smId, IPC_RMID, NULL);
            if (ctl < 0){
                printf("error on shmctl\n");
            }
            printf("In swim_mill, all children have been killed and shared memory has been destroyed (hopefully)\n");

            fclose(outFile);
            return 0;
        }

        i++;
        
        // if (k >= 2000) {
        //     for (int j = MAX_PROC; j >= 0; j--){
        //         if (processIds[j] != -1) { 
        //             kill(processIds[j], SIGUSR2);
        //             wait(NULL);
        //         }
        //     }
        // }

        // output the map to output.txt every once in a while
        k = (k+1) %5000;
        if ( k == 4999) {
                for(int row = 0; row < 10; row++){
            for(int col = 0; col < 10; col++){
                fprintf(outFile, "%c ", smPtr[(row*10)+col]);
            }
            fprintf(outFile, "\n");
            }
            fprintf(outFile, "-------\n");
        }
        

    }

    // debugging code below
    // outputs the map
    // for(int row = 0; row < 10; row++){
    //     for(int col = 0; col < 10; col++){
    //         printf("%c ", smPtr[(row*10)+col]);
    //     }
    //     printf("\n");
    // }
    // printf("\nswim mill\n");

    // clean up children and shared memory

// this section shouldn't happen but it's here just in case
    // end all children
    for (int j = MAX_PROC; j >= 0; j--){
                if (processIds[j] != -1) { 
                    kill(processIds[j], SIGUSR2);
                    wait(NULL);
                }
            }
    //

    // detach from shared memory segment
    detach = shmdt((void *) smPtr);
    if (detach < 0){
        printf("error on shmdt\n");
    }

    // delete the shared memory segment
    ctl = shmctl(smId, IPC_RMID, NULL);
    if (ctl < 0){
        printf("error on shmctl\n");
    }

    printf("swim mill end\n");
    // close the output file
    fclose(outFile);
    
return 0;
}