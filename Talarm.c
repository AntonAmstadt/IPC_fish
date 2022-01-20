#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void alarmEnd(int sig) {
    printf("alarm has ended\n");
    exit(0);
}

int main(){
    signal(SIGALRM, alarmEnd);
    alarm(2);
    while(1){
        ;
    }
    return 0;
}