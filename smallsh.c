/* smallsh.c - CS374 Assignment 3
 * 
 * Author: Ryan Grossman
 * Date: 5/2/2024
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>


typedef struct Command
{
    char* argList [512];
    int inputFd;
    int outputFd;
    int foreground;
} command;

static int foregroundOnlyMode = 0;

/**
 * Handler for SIGTSTP signal, which toggles the parent process ability to run commands in the background
 */
void handleSIGTSTP(int signo){
    switch (foregroundOnlyMode){
    case 0:
        write(STDOUT_FILENO, "\nEntering foreground-only mode (& is now ignored)\n: ", 53);
        foregroundOnlyMode = 1;
        break;
    case 1:
        write(STDOUT_FILENO, "\nExiting foreground-only mode\n: ", 33);
        foregroundOnlyMode = 0;
        break;
    default:
        perror("Program in invalid foreground access mode");
        break;
    }
    return;
}



int main(int arc, char* argv[]){
    //Setup SIGINT Ignore for Parent Process
    struct sigaction actionSIGINT = {0};
    actionSIGINT.sa_handler = SIG_IGN;
    sigfillset(&actionSIGINT.sa_mask);
    actionSIGINT.sa_flags = 0;
    sigaction(SIGINT, &actionSIGINT, NULL);

    //Setup SIGTSTP Mode Switching for Parent Process
    struct sigaction actionSIGTSTP = {0};
    actionSIGTSTP.sa_handler = handleSIGTSTP;
    sigfillset(&actionSIGTSTP.sa_mask);
    actionSIGTSTP.sa_flags = 0;
    sigaction(SIGTSTP, &actionSIGTSTP, NULL);


    char* cmdText = 0;
    size_t lenRead = 0;

    while(1){
        //Get Input command from user
        printf(": ");
        fflush(stdout);

        getline(&cmdText, &lenRead ,stdin);
        
        printf("Recieved String: '%s'", cmdText);
        
        //Reprompt on comment or newline  
        if(*cmdText == '#' || *cmdText == '\n'){ continue;}
        
    }


    return EXIT_SUCCESS;
}