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
#include <errno.h>


typedef struct Command
{
    char* argList [512];
    int inputFd;
    int outputFd;
    int foreground;
} command;

//Boolean integer representing if the shell is in foreground only mode or not
int foregroundOnlyMode = 0;

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

/**
 * Converts input from getline into array of command line arguments, replacing $$ variable with current Process PID
 */
char** parseCommandInput(char* inputText){
    char** argList = malloc(512 * sizeof(char*));
    
    //Track Current Process Pid
    char currentPid [16];
    sprintf(currentPid, "%d", getpid()); 

    const char cmdDelim[] = " \n"; //tokenize at space and newline
    char *saveptr = NULL; 
    char *cmdToken = strtok_r(inputText, cmdDelim, &saveptr); 
    int i = 0;

    while(cmdToken != NULL && i < 511){
        argList[i] = strdup(cmdToken);
        
        //Replace Every instance of '$$' with PID
        char *pidLoc = strstr(argList[i], "$$");
        while(pidLoc != NULL) {
            strcpy(pidLoc, currentPid);
            pidLoc = strstr(argList[i], "$$");
        }
        //Get Next Token
        cmdToken = strtok_r(NULL, cmdDelim, &saveptr);
        i++;
    }

    return argList;
}

/**
 * Returns enum determining type of argument passed into command line
*/
enum CommandType {
    CMD_EXIT,
    CMD_CD,
    CMD_STATUS,
    CMD_DEFAULT
};
enum CommandType getCommandType(char *arg) {
    if (strcmp("exit", arg) == 0) {
        return CMD_EXIT;
    } else if (strcmp("cd", arg) == 0) {
        return CMD_CD;
    } else if (strcmp("status", arg) == 0){
        return CMD_STATUS;
    } else {
        return CMD_DEFAULT;
    }
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

        //This handles issue where getline crashes on SIGTSTP
        ssize_t bytes_read;
        do {
            bytes_read = getline(&cmdText, &lenRead, stdin);
        } while (bytes_read == -1 && errno == EINTR);
        
        // Reprompt on comment or newline
        if (*cmdText == '#' || *cmdText == '\n') {continue;}

        //Seperate cmdText into arguments
        char** argList = parseCommandInput(cmdText);

        for(int i = 0; argList[i] != NULL && i < 511; i++){
            printf("arg[%d]: %s\n", i, argList[i]);
        }
        //Break from Main Process Loop if Error in Command
        if (*argList == NULL || strcmp("<", *argList) == 0 || strcmp(">", *argList) == 0 || strcmp("&", *argList) == 0) {break;}
        
        //Perform program requested, if Not exit, cd, or status, pass off to fork
        switch (getCommandType(*argList)){
            case CMD_EXIT:
                printf("handling exit\n");
                break;
            case CMD_CD:
                printf("handling cd\n");
                break;
            case CMD_STATUS:
                printf("handling status\n");
                break;
            default:
                printf("trying forking\n");
                break;
        }
    }
    return EXIT_SUCCESS;
}