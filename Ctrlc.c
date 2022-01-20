#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// handle ctrl + c from user
void ctrlcHandler(int sig) {
    // restore default action of ctrl + c
    signal (SIGINT, SIG_DFL);
    printf("you ctrl c'd me but i'm still going bitch!!\n");
    // send the SIGINT (ctrl + c) signal to the process that calls it
    raise(SIGINT);
}

int main(){
    signal(SIGINT, ctrlcHandler);
    while(1){
        sleep(1);
    }
    return 0;
}