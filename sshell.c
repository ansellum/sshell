#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#define CMDLINE_MAX 512
#define NUMARGS_MAX 16
#define ARGLENGTH_MAX 32

typedef struct commandObj {
        char* program;
        char* arguments[NUMARGS_MAX + 1]; // +1 for program name
        int redirectionCharacterDetected;
}commandObj;

/*Change directory*/
int changeDirectory(char *commandArguments[]) 
{
        int status;

        status = chdir(commandArguments[1]);
        if (status)
        {
                fprintf(stderr, "Error: cannot cd into directory\n");
                status = 1;
        }
        return status;
}

/*Print current working directory*/
void printWorkingDirectory() 
{
        char cwd[CMDLINE_MAX];

        getcwd(cwd, sizeof(cwd));
        printf("%s\n", cwd);
        return;
}

/*remove character from a string (outsourced)*/
char* removeChar(char *str, char charToRemmove){
    int i, j;
    int len = strlen(str);
    for(i=0; i<len; i++)
    {
        if(str[i] == charToRemmove)
        {
            for(j=i; j<len; j++)
            {
                str[j] = str[j+1];
            }
            len--;
            i--;
        }
    }
    return str;
}

/* Parses command into commandObj properties; returns number of arguments */
int parseCommand(struct commandObj* cmd, char *cmdString)
{
        char delim[] = " ";
        char copyOfToken[ARGLENGTH_MAX];
        char* token;
        char* lastArgument;
        char* fileName;
        int i = 1;
        int stopAddingTokens = 0;
        int argumentCharacterCount = 0;
        cmd->redirectionCharacterDetected = 0;

        //make editable string from literal
        char* command1 = malloc(CMDLINE_MAX * sizeof(char));
        strcpy(command1, cmdString);

        //pass program name into command object
        token = strsep(&command1, delim);
        cmd->program = token;
        cmd->arguments[0] = token;

        //fill arguments array (program name is first in argument list; see execvp() man)
        while ( command1 != NULL && strlen(command1) != 0) {
                //skip extra spaces
                while (command1[0] == ' ') token = strsep(&command1, delim);

                //update token
                token = strsep(&command1, delim);

                //pass command arguments
                if (strlen(token) == 0) break;
                //check if current token is soley the meta character '>' (i.e. 'clean' spaces around '>')
                else if (*token == '>' && strlen(token) == 1) {
                    cmd->redirectionCharacterDetected = 1;
                    continue;
                }
                //else check if current token contains the meta character '>' (i.e. no 'clean' spaces around '>')
                else {
                    //scan entire token
                    for (int n = 0; n < strlen(token); n++) {
                        //check if token contains '>'
                        if (token[n] == '>') {
                            cmd->redirectionCharacterDetected = 1;
                            //now we need to find & remove '>' and add the token to the arguments array
                            //case1: check if '>' is at the beggining of the string or at the end & remove it (i.e. '>file' or 'arg>')
                            if (n == 0 || n == (strlen(token) - 1)) {
                                token = removeChar(token, token[n]);
                                break; //break from for loop & allow revised token to be added to argument array
                            }
                            //case2: now we know '>' must be in between the last argument and the file with no spaces around it (i.e. 'arg>file')
                            else {
                                //scan entire token
                                for (int j = 0; j < strlen(token); j++) {
                                    //continue adding one to 'argumentCharacterCount' to determine how long the 'arg' is before the '>' character
                                    if (token[j] != '>') argumentCharacterCount++;
                                    else break;
                                }
                                //add string before '>' to arguments array ('arg')
                                cmd->arguments[i] = malloc(ARGLENGTH_MAX * sizeof(char));
                                //use 'argumentCharacterCount' to add the 'arg' to the arguments list
                                strncat(cmd->arguments[i], token, argumentCharacterCount);
                                i++; //next index for 'file' 
                                
                                //add string after '>' to arguments array ('file')
                                strncpy(copyOfToken, token, strlen(token) + 1); //make a copy of 'char* token' into a 'char copyOfToken[]' in order for it to be manipulated
                                cmd->arguments[i] = malloc(ARGLENGTH_MAX * sizeof(char));
                                //remove 'arg>' characters from 'arg>file' to be left with file name
                                for (int q = 0; q < argumentCharacterCount + 1; q++) {
                                    char* substr = copyOfToken + 1;
                                    memmove(copyOfToken, substr, strlen(substr) + 1);
                                }
                                //add 'copyOfToken' (i.e. the name of the file) to the arguments array
                                strcpy(cmd->arguments[i], copyOfToken);
                                //signal to stop adding entries into arguments array bc 'file' will be the last entry with output redirection
                                stopAddingTokens = 1;
                                break;
                            }
                        }
                        else continue;
                    }
                }
                
                if (stopAddingTokens == 0) {
                    //continue adding to arguments[]
                    cmd->arguments[i] = malloc(ARGLENGTH_MAX * sizeof(char));
                    strcpy(cmd->arguments[i], token);
                    i++;
                }
                else {
                    i++;
                    break;
                }
        }
        //End arguments array with NULL for execvp() detection
        cmd->arguments[i] = NULL;
        
        return i;
}

 /* Executes an external command with fork(), exec(), & wait() (phase 1)*/
void executeExternalProcess(char *cmdString)
{
        int pid;
        int childStatus;
        commandObj cmd;

        //check number of arguments & run parse
        if (parseCommand(&cmd, cmdString) > NUMARGS_MAX)
        {
                fprintf(stderr, "Error: too many process arguments\n");
                return;
        }

        //check if builtin command "cd" is called, utilizes parseCommand functionality
        if (!strcmp(cmd.program, "cd")) {
                int status = changeDirectory(cmd.arguments);
                fprintf(stderr, "+ completed '%s' [%d]\n", cmdString, status);
                return;
        }
        
        /*check if meta character '>' is used, and execute redirection 
        if (cmd.redirectionCharacterDetected == 1) {
                //printf("%s\n", "Here");
                //inputRedirection
                //return;
        }
*/
        pid = fork();
        //child process should execute the command
        if (pid == 0) {
                childStatus = execvp(cmd.program, cmd.arguments);
                exit(1); //if child reaches this line it means there was an issue running exec command
        }
        //parent process should wait for child to execute
        else if (pid > 0) {
                waitpid(pid, &childStatus, 0);
                //Check if command ran successfully (assuming no wrong arguments, a failure = command not found)
                if (childStatus) fprintf(stderr, "Error: command not found\n");
                fprintf(stderr, "+ completed '%s' [%d]\n", cmdString, childStatus);
        }
        //fork() command failed
        else exit(1);

        return;
}

int main(void)
{
        char cmdString[CMDLINE_MAX];

        while (1) {
                char *nl;
       
                /* Print prompt */
                printf("sshell@ucd$ ");
                fflush(stdout);

                /* Get command line */
                fgets(cmdString, CMDLINE_MAX, stdin);

                /* Print command line if stdin is not provided by terminal */
                if (!isatty(STDIN_FILENO)) {
                        printf("%s", cmdString);
                        fflush(stdout);
                }

                /* Remove trailing newline from command line */
                nl = strchr(cmdString, '\n');
                if (nl) *nl = '\0';

                /* Builtin command "exit"*/
                if (!strcmp(cmdString, "exit")) {
                        fprintf(stderr, "Bye...\n");
                        fprintf(stderr, "+ completed '%s' [%d]\n",cmdString, 0);
                        break;
                }
                /* Builtin command "pwd"*/
                else if (!strcmp(cmdString, "pwd"))
                {
                        printWorkingDirectory();
                        fprintf(stderr, "+ completed '%s' [0]\n", cmdString);
                }
                else executeExternalProcess(cmdString);
        }
       
        return EXIT_SUCCESS;
}