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
#include <errno.h>
#include <ctype.h>

typedef struct Command
{
    char* argList [512];
    int inputFd;
    int outputFd;
    int foreground;
} command;

typedef struct ProcessList {
	pid_t pid;
	int inputFd;
	int outputFd;
	struct ProcessList* next;
} ProcessList;

//Boolean integer representing if the shell is in foreground only mode or not
int foregroundOnlyMode = 0;
/**
 * Checks every process held in the backgroundprocesses list, and If they have completed, print their exiting information and remove them from the list
*/
void killFinishedProcesses(ProcessList **head){
    ProcessList *current = *head;
    ProcessList *prev = NULL;
    
    while(current != NULL){
        int status;
        pid_t result = waitpid(current->pid, &status, WNOHANG);

        switch (result){
        case -1:
            perror("Waitpid");
            exit(EXIT_FAILURE);
        case 0:
            //Process not finished
            prev = current;
            current = current->next;
            break;
        default:
            //Process Finished
            if(WIFSIGNALED(status)){
                printf("Process %d was SIGNALED with %d\n", current->pid, WTERMSIG(status));
            } else {
                printf("Process %d exited with status %d\n", current->pid, WEXITSTATUS(status));
            }
            // Remove finished process from list
            if (prev == NULL) {
                // Head of list
                *head = current->next;
                free(current);
                current = *head;
            } else {
                prev->next = current->next;
                free(current);
                current = prev->next;
            }
            break;
        }
    }
    return;
}
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
 * Frees an array of command line arguments
*/

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
        if(isprint(*cmdToken)){
            argList[i] = strdup(cmdToken);
        }   
        
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
enum ErrorType {
    NO_ERROR = 0,
    NO_INPUT_FILE = 1,
    NO_OUTPUT_FILE = 2,
    FILE_OPEN_FAIL = 3,
};
enum ErrorType commandStructCreate(command* command, char* argList [512]){
    //Set command structs arglist to NULL for length of command line arguments
    memset(command->argList, 0, sizeof(command->argList));
    
    // printf("Initialized to null:\n");
    // for (int i = 0; command->argList[i] != NULL; i++) {
    //         printf("arglist[%d]: %s\n", i, command->argList[i]);
    // }
    // printf("Finished\n");
    //Default Command Setup
    command->inputFd = STDIN_FILENO;
	command->outputFd = STDOUT_FILENO;
	command->foreground = 1;
    //Iterate through argList to populate structs arglist, and update standard input/output setup if necessary
    enum ErrorType commandErrors = NO_ERROR;
    int commandNum = 0;
    int i = 0;
    while((argList[i] != NULL) && (commandErrors == NO_ERROR)){
        //Set Input File
        if(strcmp("<", argList[i]) == 0){
            //Expect another Argument as filename
            if(argList[i+1] == NULL){
                commandErrors = NO_INPUT_FILE; 
                break;
            }
            //Try to open return error if failed
            int inputFd = open(argList[i+1], O_RDONLY);
            if(inputFd < 0){
                commandErrors = FILE_OPEN_FAIL;
                break;
            }
            command->inputFd = inputFd;
            i += 2; //Increment by both {<} and filename
        }
        //Set Output File
        else if(strcmp(">", argList[i]) == 0){
            //Expect another argument as Filename
            if(argList[i+1] == NULL){
                commandErrors = NO_OUTPUT_FILE;
                break;
            }
            //Try to open return error if failed
            int outputFd = open(argList[i+1], O_CREAT | O_TRUNC | O_WRONLY, 0640);
            if (outputFd < 0){
                commandErrors = FILE_OPEN_FAIL;
                break;
            }
            command->outputFd = outputFd;
            i += 2; //Increment by both {>} and filename
        }
        //Set Background Command if & final argument and shell not in foreground only mode
        else if(strcmp("&", argList[i]) == 0 && argList[i+1] == NULL){
            //If It can run background processes, set it as such
            if(!foregroundOnlyMode){
                command->foreground = 0;
            }
            i++; // Increment index in either case to prevent unexpected token in execvp args

        }
        //If none of those cases apply, append the argument to the first non-null spot in the command struct
        else {
            command->argList[commandNum] = argList[i];
            // printf("newArg[%d]: %s\n", commandNum, command->argList[commandNum]);
            commandNum++;
            i++;

        }

    }
    // printf("Finished Parsing Struct\n");
    // for (int i = 0; command->argList[i] != NULL; i++) {
    //         printf("arglist[%d]: %s\n", i, command->argList[i]);
    // }
    // printf("Read Everything\n");
    //Redirect input and output to dev/null for background processes if not already redirected
    if(!command->foreground && command->inputFd == STDIN_FILENO) { command->inputFd = open("/dev/null", O_RDONLY);}
    if(!command->foreground && command->outputFd == STDOUT_FILENO) { command->outputFd = open("/dev/null", O_WRONLY);}

    return commandErrors;
}

/**
 * Creates a New Process and pushes it to the end of the list
*/
void pushNewProcess(ProcessList **curList, pid_t pid, int inputFd, int outputFd){
    //Create new Process
    ProcessList *newProcess = malloc(sizeof(ProcessList));
	newProcess->pid = pid;
	newProcess->next = NULL;
	newProcess->inputFd = inputFd;
	newProcess->outputFd = outputFd;

    if (*curList == NULL) {
        *curList = newProcess;
    } else {
        // Traverse to the end of the list and add the new process
        ProcessList *iterator = *curList;
        while (iterator->next != NULL) {
            iterator = iterator->next;
        }
        iterator->next = newProcess;
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


    char* cmdText = NULL;
    size_t lenRead = 0;
    int lastForegroundStatus = 0;
    int terminatedBySignal = 0;
    ProcessList *backgroundProcessList = NULL;

    while(1){
        //Check and Remove any Finished Processes
        killFinishedProcesses(&backgroundProcessList);
        
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
        
        //Break from Main Process Loop if Fatal Error in command (No arguments, or first argument >, <, or &)
        if (*argList == NULL || strcmp("<", *argList) == 0 || strcmp(">", *argList) == 0 || strcmp("&", *argList) == 0) {break;}
        
        //Perform program requested, if Not exit, cd, or status, pass off to fork
        switch (getCommandType(*argList)){
            case CMD_EXIT:
                //Free all memory and exit program gracefully
                freeArgs(argList);
                while (backgroundProcessList != NULL) {
                    ProcessList *temp = backgroundProcessList;
                    backgroundProcessList =  backgroundProcessList->next;
                    free(temp);
                }
                free(cmdText);
                return EXIT_SUCCESS;
            case CMD_CD:
            {
                char* startDir = getcwd(NULL, 0);
                char* homeLink = getenv("HOME");
                
                //When Passed No arguments, changes directory to home
                if (argList[1] == NULL) {
					if (chdir(homeLink) == 0) {
						setenv("PWD", homeLink, 1);
                    }
				} else {
                    //Otherwise, attempts to change to directory specified by arg1, 
                    char* newDir = argList[1];
                    
                    if (chdir(newDir) == 0) {
						char *currDir = getcwd(NULL, 0);
						setenv("PWD", currDir, 1);
						free(currDir);
					} else {
                        //Occours when ERRNO is ENOTDIR
                        printf("Cannot Change to %s: Not a Directory\n", newDir);
                    }
                }
                free(startDir);
                fflush(stdout);
            }
                break;
            case CMD_STATUS:
                //Output Exiting status or killing signal of most recent foreground processs
                if(terminatedBySignal){
                    printf("Last Foreground Process Signaled with %d", lastForegroundStatus);
                } else{
                    printf("Last Foreground Process Exited with status %d\n", lastForegroundStatus);
                }
                break;
            default:
            {
                command newCommand;
                enum ErrorType cmdErrors = commandStructCreate(&newCommand, argList);
                switch(cmdErrors){
                    case NO_INPUT_FILE:
                        printf("Expected Filename After <\n");
                        continue;
                    case NO_OUTPUT_FILE:
                        printf("Expected Filename after >\n");
                        continue;
                    case FILE_OPEN_FAIL:
                        printf("File could not be opened\n");
                        continue;
                }
                // Testing Proper Struct Creation
                // printf("Arguments:\n");
                // for (int i = 0; i < 512 && newCommand.argList[i] != NULL ; ++i) {
                //     printf("argList[%d]: %s\n", i, newCommand.argList[i]);
                // }
                // printf("Input File Descriptor: %d\n", newCommand.inputFd);
                // printf("Output File Descriptor: %d\n", newCommand.outputFd);
                // printf("Foreground: %d\n", newCommand.foreground);
                if(cmdErrors == NO_ERROR){
                    // Fork and exec new command
                    pid_t childCmd = fork();
                    switch(childCmd){
                        case -1: 
                            perror("Abort: Forking Error");
                            exit(EXIT_FAILURE);
                        case 0:
                            // Child Ignores SIGTSTP
                            actionSIGTSTP.sa_handler = SIG_IGN;
							actionSIGTSTP.sa_flags = 0;
							sigaction(SIGTSTP, &actionSIGTSTP, NULL);

							if (newCommand.foreground) {
								//If Child is in foreground, reset SIGINT to Default using SIGDFL
								actionSIGINT.sa_handler = SIG_DFL;
								sigaction(SIGINT, &actionSIGINT, NULL);
							}
							//Redirect Input and Output to struct values
							if (dup2(newCommand.inputFd, STDIN_FILENO) == -1) { 
                                printf("IO Redirection Failed");
                                exit(EXIT_FAILURE); }
							if (dup2(newCommand.outputFd, STDOUT_FILENO) == -1) { 
                                printf("IO Redirection Failed"); 
                                exit(EXIT_FAILURE); }
                            
                            //Execute Command
                            execvp(newCommand.argList[0], newCommand.argList);
                            perror("execvp error");
                            //Cleanup on Execvp Error
                            while (backgroundProcessList != NULL) {
								ProcessList *temp = backgroundProcessList;
								backgroundProcessList =  backgroundProcessList->next;
								free(temp);
							}
                            freeArgs(argList);
                            free(cmdText);

                            exit(EXIT_FAILURE); // Process kills itself
                        default:
                            //If newCommand is Foreground, Block and wait
                            if(newCommand.foreground){
                                int childStatus;
                                waitpid(childCmd, &childStatus, 0);
                                //Update lastforegoundstatus
                                if(terminatedBySignal = WIFSIGNALED(childStatus)){
                                    lastForegroundStatus = WTERMSIG(childStatus);
                                } else {
                                    lastForegroundStatus = WEXITSTATUS(childStatus);
                                }   
                                //Close Open File Descriptors
                                if (newCommand.inputFd != STDIN_FILENO) { close(newCommand.inputFd); }
								if (newCommand.outputFd != STDOUT_FILENO) { close(newCommand.outputFd); }


                            } else {
                                //Otherwise, print PID of child and add to list of ongoing processes
                                printf("New Background Process with PID[%d]\n", childCmd);
                                pushNewProcess(&backgroundProcessList, childCmd, newCommand.inputFd, newCommand.outputFd);
                            }    
                    }
                }
            }
                break;
        }
    }
    return EXIT_SUCCESS;
}