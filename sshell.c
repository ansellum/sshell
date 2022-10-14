#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "stringstack.h"

#define ARGLENGTH_MAX 32
#define CMDLINE_MAX 512
#define NUM_DELIMS 3
#define NUMARGS_MAX 16
#define PATH_MAX 4096
#define PIPES_MAX 4
#define STACK_SIZE 104

int oRedirectSymbolDetected = 0;
int iRedirectSymbolDetected = 0;
char* oFile = "";
char* iFile = "";

typedef struct commandObj
{
        char* program;
        char* arguments[NUMARGS_MAX + 1]; // +1 for program name
        int numArgs;
}commandObj;

/*Redirect*/
void redirectOutput(int fd)
{
        fd = open(oFile, O_CREAT | O_WRONLY | O_TRUNC); //Create, write-only, truncate if exist
        if (fd < 0) return; //open() is unsuccessful
        dup2(fd, STDOUT_FILENO);
}

void redirectInput(int fd)
{
        fd = open(iFile, O_RDONLY); // read only
        if (fd < 0) return; //open() is unsuccessful
        dup2(fd, STDIN_FILENO);
}

int changeDirectory(char* directory)
{
        int status = 0;

        if (chdir(directory)) status = 1;
        return status;
}

int printWorkingDirectory()
{
        char cwd[PATH_MAX];

        if (getcwd(cwd, sizeof(cwd)) == NULL) return 1;
        fprintf(stdout, "%s\n", cwd);
        return 0;
}

/*Executes an external command with fork(), execvp(), & waitpid() */
void executePipeline(int fd[][2], int exitval[], commandObj* cmd, int numPipes, const int index)
{
        int status;
        int pid = fork();

        if (pid == 0) { //Child process

                if (index > 0)                          dup2(fd[index - 1][0], STDIN_FILENO);   //redirect stdin to read pipe,  unless it's the first command
                else if (iRedirectSymbolDetected)       redirectInput(fd[index][0]);

                if (index < numPipes)                   dup2(fd[index][1], STDOUT_FILENO);      //redirect stdout to write pipe, unless it's the last command
                else if (oRedirectSymbolDetected)       redirectOutput(fd[index][1]);               //check if filename is detected, and execute redirection

                //close all pipes
                for (int i = 0; i < numPipes; ++i)
                {
                        close(fd[i][0]);
                        close(fd[i][1]);
                }
                //execute program
                execvp(cmd[index].program, cmd[index].arguments);
                if (errno = ENOENT) fprintf(stderr, "Error: command not found\n");
                exit(EXIT_FAILURE);
        }
        else if (pid > 0) { //Parent process
                if (index < numPipes) executePipeline(fd, exitval, cmd, numPipes, index + 1);

                //close all pipes
                for (int i = 0; i < numPipes; ++i)
                {
                        close(fd[i][0]);
                        close(fd[i][1]);
                }
        }
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) exitval[index] = WEXITSTATUS(status);
}

int parseCommand(const int index, commandObj* cmd, char* command1)
{
        int argIndex = 1;
        char* token;

        /*PARSE PROPERTIES*/
        while (command1[0] == ' ') command1++;
        token = strsep(&command1, " ");

        cmd[index].program = token;
        cmd[index].arguments[0] = token;
        cmd[index].numArgs = 0;

        while (command1 != NULL && strlen(command1) != 0) {
                //Pre-token error processing
                if (argIndex >= NUMARGS_MAX) return EXIT_FAILURE;   //Error: Too many arguments

                //skip extra spaces
                while (command1[0] == ' ') command1++;

                token = strsep(&command1, " ");

                //Post-token error checking
                if (strlen(token) == 0) break;

                //pass command arguments
                cmd[index].arguments[argIndex] = (char*)malloc(ARGLENGTH_MAX * sizeof(char));
                strcpy(cmd[index].arguments[argIndex], token);
                cmd[index].numArgs = argIndex++;
        }
        cmd[index].arguments[argIndex] = NULL; //execvp()

        return EXIT_SUCCESS;
}

/* Parses command into commandObj properties. Returns # of cmd objects, -1 if too many arguments in one command*/
int parseDelimiters(const int index, commandObj* cmd, char* cmdString)
{
        int numPipes = index;

        char* command1 = malloc(CMDLINE_MAX * sizeof(char));
        char* command2 = malloc(CMDLINE_MAX * sizeof(char));
        char* delims[NUM_DELIMS] = { "<" , ">" , "|" };

        strcpy(command1, cmdString);

        /*PARSE DELIMITERS*/
        for (int i = 0; i < NUM_DELIMS; ++i)
        {
                command2 = command1;
                command1 = strsep(&command2, delims[i]);

                if (command2 == NULL) continue; //Nothing Found
                while (command2[0] == ' ') command2++;

                if (strlen(command1) == 0 || (strlen(command2) == 0 && i == 2)) return -1; //Error: Missing command

                switch (i) {
                case 0: // < input redirection
                        if (strlen(command2) == 0)              return -2; //Error: no input file
                        if (strchr(command2, '|') != NULL)      return -3; //Error: mislocated input redirection

                        access(command2, R_OK);
                        if (errno == ENOENT)                     return -4; //Error: cannot open input file (does not exist)

                        iRedirectSymbolDetected = 1;
                        iFile = command2;
                        while (iFile[0] == ' ') iFile++;
                        break;

                case 1: // > output redirection
                        if (strlen(command2) == 0)              return -5; //Error: no output file
                        if (strchr(command2, '|') != NULL)      return -6; //Error: mislocated output redirection

                        access(command2, W_OK);
                        if (errno == EACCES)                    return -7; //Error: cannot open output file (permission denied)

                        oRedirectSymbolDetected = 1;
                        oFile = command2;
                        while (oFile[0] == ' ') oFile++;
                        break;

                case 2: // | piping
                        numPipes = parseDelimiters(index + 1, cmd, command2);
                        break;
                }
        }
        if (parseCommand(index, cmd, command1) != 0) return -8; //Pass everything BEFORE delim

        return numPipes;
}

int errorManagement(int error)
{
        switch (error) {
        case -1: // > file 
                fprintf(stderr, "Error: missing command\n");
                return 1;

        case -2: // sort <
                fprintf(stderr, "Error: no input file\n");
                return 1;

        case -3: // < before |
                fprintf(stderr, "Error: mislocated input redirection\n");
                return 1;

        case -4:
                fprintf(stderr, "Error: cannot open input file \n");
                return 1;

        case -5: // echo >
                fprintf(stderr, "Error: no output file\n");
                return 1;

        case -6: // > before |
                fprintf(stderr, "Error: mislocated output redirection\n");
                return 1;

        case -7: // file does not exist
                fprintf(stderr, "Error: cannot open output file \n");
                return 1;

        case -8: // $ ls 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16
                fprintf(stderr, "Error: too many process arguments\n");
                return 1;
        }

        return 0;
}

/*Commands that need special instruction.*/
int specialCommands(commandObj cmd, char* cmdString, stringStack *directoryStack)
{
        //Built-in Commands
        int error;

        if (!strcmp(cmd.program, "cd"))
        {
                error = changeDirectory(cmd.arguments[1]);

                if (error)      fprintf(stderr, "Error: cannot cd into directory\n");

                fprintf(stderr, "+ completed '%s' [%d]\n", cmdString, error);
                return 1; 
        }
        if (!strcmp(cmd.program, "pwd"))
        {
                error = printWorkingDirectory();

                fprintf(stderr, "+ completed '%s' [%d]\n", cmdString, error);
                return 1;
        }
        if (!strcmp(cmd.program, "exit"))
        {
                fprintf(stderr, "Bye...\n");
                fprintf(stderr, "+ completed '%s' [%d]\n", cmdString, EXIT_SUCCESS);
                exit(EXIT_SUCCESS);
        }

        //Directory Stack
        if (!strcmp(cmd.program, "pushd"))
        {
                char cwd[PATH_MAX];

                if (getcwd(cwd, sizeof(cwd)) == NULL)
                {
                        fprintf(stderr, "Error: couldn't get current working directory\n");
                        fprintf(stderr, "+ completed '%s' [%d]\n", cmdString, EXIT_FAILURE);
                        return 1;
                }
                
                error = changeDirectory(cmd.arguments[1]);
                if (error)
                {
                        fprintf(stderr, "Error: no such directory\n"); //directory probably deleted
                        fprintf(stderr, "+ completed '%s' [%d]\n", cmdString, error);
                        return 1;
                }

                error = push(directoryStack, cwd);
                if (error)
                {
                        fprintf(stderr, "Error: directory stack full\n");
                }

                fprintf(stderr, "+ completed '%s' [%d]\n", cmdString, error);
                return 1;
        }        
        if (!strcmp(cmd.program, "popd"))
        {
                char* prevDirectory;

                if (cmd.arguments[1] != NULL)
                {
                        fprintf(stderr, "Error: extra operant '%s'\n", cmd.arguments[1]);
                        fprintf(stderr, "+ completed '%s' [%d]\n", cmdString, EXIT_FAILURE);
                        return 1;
                }

                prevDirectory = pop(directoryStack);
                if (prevDirectory == NULL)
                {
                        fprintf(stderr, "Error: directory stack empty\n");
                        fprintf(stderr, "+ completed '%s' [%d]\n", cmdString, EXIT_FAILURE);
                        return 1;
                }

                error = changeDirectory(prevDirectory);
                if (error)
                {
                        fprintf(stderr, "Error: cannot cd into directory\n");
                }

                fprintf(stderr, "+ completed '%s' [%d]\n", cmdString, error);
                return 1;
        }
        if (!strcmp(cmd.program, "dirs"))
        {
                if (cmd.arguments[1] != NULL)
                {
                        fprintf(stderr, "Error: extra operant '%s'\n", cmd.arguments[1]);
                        fprintf(stderr, "+ completed '%s' [%d]\n", cmdString, EXIT_FAILURE);
                        return 1;
                }

                error = printWorkingDirectory();

                for (int i = directoryStack->top; i >= 0; --i) fprintf(stdout, "%s\n", directoryStack->items[i]);
                fprintf(stderr, "+ completed '%s' [%d]\n", cmdString, error);
                return 1;
        }
        return 0;
}

void prepareExternalProcess(char *cmdString, stringStack *directoryStack)
{
        int numPipes;
        commandObj cmd[PIPES_MAX];

        if (strlen(cmdString) == 0) return;

        /*Parse*/
        numPipes = parseDelimiters(0, cmd, cmdString);

        /*Post-parse error management*/
        if(errorManagement(numPipes) != EXIT_SUCCESS) return;

        /*Commands that require specific attention*/
        if (specialCommands(cmd[0], cmdString, directoryStack) != EXIT_SUCCESS) return;

        /*Piping (works with single commands)*/
        int fd[numPipes][2];
        int exitval[numPipes + 1];

        for (int i = 0; i < numPipes; ++i)
        {
                if (pipe(fd[i]) != 0)
                        exit(EXIT_FAILURE);
        }
        executePipeline(fd, exitval, cmd, numPipes, 0);

        oRedirectSymbolDetected = 0;
        iRedirectSymbolDetected = 0;
        oFile = "";
        iFile = "";

        fprintf(stderr, "+ completed '%s' ", cmdString);
        for (int i = 0; i < numPipes + 1; ++i)
        {
                fprintf(stderr, "[%d]", exitval[i]);  //Print exit values
        }
        fprintf(stderr, "\n");

        return;
}

int main(void)
{
        char cmdString[CMDLINE_MAX];
        stringStack *directoryStack = newStack(STACK_SIZE);

        oFile = (char*)malloc(ARGLENGTH_MAX * sizeof(char));
        iFile = (char*)malloc(ARGLENGTH_MAX * sizeof(char));

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

                prepareExternalProcess(cmdString, directoryStack);
        }
       
        return EXIT_SUCCESS;
}