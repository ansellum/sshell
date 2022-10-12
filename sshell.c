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
#define PIPES_MAX 4
#define NUM_DELIMS 3

int oRedirectSymbolDetected = 0;
int iRedirectSymbolDetected = 0;
char* oFile = "";
char* iFile = "";

typedef struct commandObj {
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

/*Change directory*/
int changeDirectory(char *directory)
{
        int status;

        status = chdir(directory);
        return status;
}

/*Print current working directory*/
int printWorkingDirectory()
{
        char cwd[CMDLINE_MAX];

        if (getcwd(cwd, sizeof(cwd)) == NULL) return 1;
        printf("%s\n", cwd);
        return 0;
}

void executePipeline(int fd[][2], int exitval[], struct commandObj* cmd, int numPipes, const int index)
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

/* Parses command into commandObj properties. Returns # of cmd objects, -1 if too many arguments in one command*/
int parseCommand(const int index, struct commandObj* cmd, char* cmdString)
{
        int argIndex = 1;
        int numPipes = index;
        char* command1 = malloc(CMDLINE_MAX * sizeof(char));
        char* command2 = malloc(CMDLINE_MAX * sizeof(char));
        char* delims[NUM_DELIMS] = { "<" , ">" , "|" };
        char* token;

        strcpy(command1, cmdString);
       
        /*PARSE DELIMITERS*/
        for (int i = 0; i < NUM_DELIMS; ++i)
        {
                command2 = command1;
                command1 = strsep(&command2, delims[i]);

                if (command2 == NULL) continue; //Nothing Found

                if (strlen(command1) == 0 || (strlen(command2) == 0 && i == 2)) return -1; //Error: Missing command

                switch (i) {
                case 0: // < input redirection
                        if (strlen(command2) == 0)              return -2; //Error: no input file
                        if (strchr(command2, '|') != NULL)      return -3; //Error: mislocated input redirection
                        if (access(command2, R_OK) != 0)        return -4; //Error: cannot open input file

                        iRedirectSymbolDetected = 1;
                        iFile = command2;
                        while (iFile[0] == ' ') iFile++;
                        break;

                case 1: // > output redirection
                        if (strlen(command2) == 0)              return -5; //Error: no output file
                        if (strchr(command2, '|') != NULL)      return -6; //Error: mislocated output redirection
                        if (access(command2, W_OK) != 0)        return -7; //Error: cannot open output file

                        oRedirectSymbolDetected = 1;
                        oFile = command2;
                        while (oFile[0] == ' ') oFile++;
                        break;

                case 2: // | piping
                        numPipes = parseCommand(index + 1, cmd, command2);
                        break;
                }
        }

        /*PARSE COMMAND PROPERTIES*/
        while (command1[0] == ' ') command1++;
        token = strsep(&command1, " ");

        cmd[index].program = token;
        cmd[index].arguments[0] = token;
        cmd[index].numArgs = 0;

        while ( command1 != NULL && strlen(command1) != 0) {
                //Pre-token error processing
                if (argIndex >= NUMARGS_MAX) return -8;   //Error: Too many arguments

                //skip extra spaces
                while (command1[0] == ' ') command1++;

                token = strsep(&command1, " ");

                //Post-token error checking
                if (strlen(token) == 0) break;

                //pass command arguments
                cmd[index].arguments[argIndex] = malloc(ARGLENGTH_MAX * sizeof(char));
                strcpy(cmd[index].arguments[argIndex], token);
                cmd[index].numArgs = argIndex++;
        }
        cmd[index].arguments[argIndex] = NULL; //execvp()
        
        return numPipes;
}

int builtin_commands(struct commandObj cmd, char* cmdString)
{
        int status = 0;
        if (!strcmp(cmd.program, "cd"))
        {
                status = changeDirectory(cmd.arguments[1]);
                if (status)
                {
                        fprintf(stderr, "Error: cannot cd into directory\n");
                        status = 1;
                }
                fprintf(stderr, "+ completed '%s' [%d]\n", cmdString, status);
                return status;
        }
        else if (!strcmp(cmd.program, "pwd"))
        {
                status = printWorkingDirectory();
                fprintf(stderr, "+ completed '%s' [%d]\n", cmdString, status);
                return status;
        }
        else if (!strcmp(cmd.program, "exit"))
        {
                fprintf(stderr, "Bye...\n");
                fprintf(stderr, "+ completed '%s' [%d]\n", cmdString, EXIT_SUCCESS);
                exit(EXIT_SUCCESS);
        }
        return status;
}

 /*Executes an external command with fork(), exec(), & wait() (phase 1)*/
void prepareExternalProcess(char *cmdString)
{
        int numPipes;
        oFile = malloc(ARGLENGTH_MAX * sizeof(char));
        iFile = malloc(ARGLENGTH_MAX * sizeof(char));
        commandObj cmd[PIPES_MAX];

        if (strlen(cmdString) == 0) return;

        /*Parse*/
        numPipes = parseCommand(0, cmd, cmdString);

        /*Post-parse error management*/
        switch (numPipes) {
        case -1: // > file 
                fprintf(stderr, "Error: missing command\n");
                return;

        case -2: // sort <
                fprintf(stderr, "Error: no input file\n");
                return;

        case -3: // < before |
                fprintf(stderr, "Error: mislocated input redirection\n");
                return;   

        case -4: 
                fprintf(stderr, "Error: cannot open input file \n");
                return;

        case -5: // echo >
                fprintf(stderr, "Error: no output file\n");
                return;

        case -6: // > before |
                fprintf(stderr, "Error: mislocated output redirection\n");
                return;

        case -7:
                fprintf(stderr, "Error: cannot open output file \n");
                return;

        case -8: // $ ls 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16
                fprintf(stderr, "Error: too many process arguments\n");
                return;
        }      

        /* Builtin commands*/
        if (builtin_commands(cmd[0], cmdString)) return;

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

                prepareExternalProcess(cmdString);
        }
       
        return EXIT_SUCCESS;
}