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
#include <sys/wait.h>
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

void freeArgs(char** argList){
	char* currentArg = argList[0];
    for (int i = 0; currentArg != NULL && i < 511; i++) {
		free(currentArg);
        currentArg = argList[++i];
	}
    free(argList);
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
int commandStructCreate(command* command, char* argList [512], int foregroundOnlyMode){
    //Set command structs arglist to NULL for length of command line arguments
    for (char **argIdx = command->argList; *argIdx != NULL; argIdx++) {
        *argIdx = NULL;
    }
    //Standard Command Setup
    command->inputFd = STDIN_FILENO;
	command->outputFd = STDOUT_FILENO;
	command->foreground = 1;
    //Iterate through argList to populate structs arglist, and update standard input/output setup if necessary
    int commandErrors = 0;
    while(0){
        // TODO: Finish iterating through arglist
    }
    return commandErrors;
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

        // Testing cmdline arg parsing
        // for(int i = 0; argList[i] != NULL && i < 511; i++){
        //     printf("arg[%d]: %s\n", i, argList[i]);
        // }
        //Break from Main Process Loop if Error in Command
        if (*argList == NULL || strcmp("<", *argList) == 0 || strcmp(">", *argList) == 0 || strcmp("&", *argList) == 0) {break;}
        
        //Perform program requested, if Not exit, cd, or status, pass off to fork
        switch (getCommandType(*argList)){
            case CMD_EXIT:
                freeArgs(argList);
                //TODO: Kill all background processes
                
                free(cmdText);
                return 0;
            case CMD_CD:
                char* startDir = getcwd(NULL, 0);
                char* homeLink = getenv("HOME");
                
                //When Passed No arguments, changes directory to home
                if (argList[1] == NULL) {
					if (chdir(homeLink) == 0) {
						setenv("PWD", homeLink, 1);
                        printf("WAS %s\n", startDir);
                        printf("IS %s\n", getcwd(NULL, 0));
                    }
				} else {
                    //Otherwise, attempts to change to directory specified by arg1, 
                    char* newDir = argList[1];
                    
                    if (chdir(newDir) == 0) {
						char *currDir = getcwd(NULL, 0);
						setenv("PWD", currDir, 1);
						printf("WAS %s\n", startDir);
                        printf("IS %s\n", currDir);
						free(currDir);
					} else {
                        printf("Cannot Change to %s: Not a Directory\n", newDir);
                    }
                }
                free(startDir);
                fflush(stdout);
                break;
            case CMD_STATUS:
                printf("handling status\n");
                break;
            default:
                command newCommand;
                int cmdErrors = commandStructCreate(&newCommand, argList, foregroundOnlyMode);
                if(cmdErrors != 0 ){
                    // TODO: Output error message for different types of Errors: expecting filename after redirect, could't open files, unexpected token, etc
                } else {
                    // Fork and exec new command
                    pid_t childCmd = fork();
                    switch(childCmd){
                        case -1: 
                            perror("Abort: Forking Error");
                            exit(EXIT_FAILURE);
                        case 0:
                            // Child Process actions
                            printf("Im The child %d, I'm killing myself", getpid());
                            exit(EXIT_SUCCESS); // Immediately kill yourself
                        default:
                            //If newCommand is Foreground, Block and wait
                            if(newCommand.foreground){
                                int childStatus;
                                waitpid(childCmd, &childStatus, 0);
                                printf("My Child Has Died with status %d", childStatus);
                            } else {
                                //Otherwise, print PID of child and add to list of ongoing processes
                                printf("New Background Process with PID[%d]", childCmd);
                                //TODO: Add To List of ongoing processes
                            }    
                    }
                }
                break;
        }
        // freeArgs(argList);
    }
    return EXIT_SUCCESS;
}