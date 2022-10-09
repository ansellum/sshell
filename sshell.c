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

typedef struct commandObj {
        char* program;
        char* arguments[NUMARGS_MAX + 1]; // +1 for program name
        int numArgs;
        int order;
}commandObj;

/*Debug function*/
void printCommands(struct commandObj* cmd, int index)
{
        printf("------------------------------\n");
        for (int i = 0; i <= index; ++i)
        {
                printf("Program \t%d: %s\n", i, cmd[i].program);
                printf("Arguments \t%d: ", i);
                for (int j = 1; j <= cmd[i].numArgs; ++j) printf("[%s] ", cmd[i].arguments[j]);
                printf("\n");
        }
        printf("------------------------------\n\n");
}

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
int printWorkingDirectory() 
{
        char cwd[CMDLINE_MAX];

        if (getcwd(cwd, sizeof(cwd)) == NULL) return 1;
        printf("%s\n", cwd);
        return 0;
}

/* Parses command into commandObj properties. Returns # of cmd objects, -1 if too many arguments in one command*/
int parseCommand(const int index, struct commandObj* cmd, char* cmdString)
{
        char delim[] = " ";
        char* token;
        int numPipes = index;
        int argIndex = 1;

        //make editable string from literal
        char* command1 = malloc(CMDLINE_MAX * sizeof(char));
        char* command2 = malloc(CMDLINE_MAX * sizeof(char));
        strcpy(command1, cmdString);

        /*Recusively parse pipes*/
        command2 = command1;
        command1 = strsep(&command2, "|"); //command1 = first command, command2 = rest of the cmdline
        if(command2 != NULL) numPipes = parseCommand(index + 1, cmd, command2);

        /*Define command struct*/
        while (command1[0] == ' ') command1++;
        token = strsep(&command1, delim);
        cmd[index].program = token;
        cmd[index].arguments[0] = token;
        cmd[index].numArgs = 0;
        cmd[index].order = index;

        //iterate through arguments
        while ( command1 != NULL && strlen(command1) != 0) {
                /*Error checking*/
                if (argIndex >= NUMARGS_MAX) return -1;   //Too many arguments

                //skip extra spaces
                while (command1[0] == ' ') token = strsep(&command1, delim);

                //update token
                token = strsep(&command1, delim);

                //pass command arguments
                cmd[index].arguments[argIndex] = malloc(ARGLENGTH_MAX * sizeof(char));
                strcpy(cmd[index].arguments[argIndex], token);
                cmd[index].numArgs = argIndex++;
        }
        //End arguments arr with NULL for execvp() detection
        cmd[index].arguments[argIndex] = NULL;

        return numPipes;
}

void executePipeline(int* fd[], struct commandObj* cmd, const int numPipes, const int index)
{ 
        int pid = fork();

        if (pid == 0) { //Child process 
                if (index != numPipes)  dup2(fd[index][1], STDOUT_FILENO);      //Last program: only redirect read
                if (index != 0)         dup2(fd[index][0], STDIN_FILENO);       //First program: only redirect write

                //close all pipes
                for (i = 0; i < numPipes; ++i)
                {
                        close(fd[i][0]);
                        close(fd[i][1]);
                }
                //execute program
                execvp(cmd[index].program, cmd[index].arguments);
        }
        else if (pid > 0 && index < numPipes) executePipeline(fd, cmd, numPipes, index + 1);
}

 /*Executes an external command with fork(), exec(), & wait() (phase 1)*/
void prepareExternalProcess(char *cmdString)
{
        int numPipes;
        commandObj cmd[PIPES_MAX];

        /*Parse and check for errors*/
        numPipes = parseCommand(0, cmd, cmdString) + 1;
        if (numPipes < 0)
        {
                fprintf(stderr, "Error: too many process arguments\n");
                return;
        }
        /*Single Command*/
        else if (numPipes == 0)
        {
                /* Builtin commands*/
                if (!strcmp(cmd[0].program, "cd")) {
                        int status = changeDirectory(cmd[0].arguments);
                        fprintf(stderr, "+ completed '%s' [%d]\n", cmdString, status);
                        return;
                }
                else if (!strcmp(cmd[0].program, "pwd"))
                {
                        int status = printWorkingDirectory();
                        fprintf(stderr, "+ completed '%s' [%d]\n", cmdString, status);
                        return;
                }
                else if (!strcmp(cmd[0].program, "exit")) {
                        fprintf(stderr, "Bye...\n");
                        fprintf(stderr, "+ completed '%s' [%d]\n", cmdString, EXIT_SUCCESS);
                        exit(EXIT_SUCCESS);
                }

                //need exec
        }
        /*Pipes*/
        else
        {
                //create a R/W fd for each pipe
                int fd[numPipes][2];
                for (int i = 0; i < numPipes, ++i) {
                        if (pipe(fd[i]) != 0)
                                exit(EXIT_FAILURE);
                }

                executePipeline(fd, cmd, numPipes, i);
        }
        //Debug command objects
        //printCommands(cmd, numObjects); 

        return;
}

int main(void)
{
        char cmdString[CMDLINE_MAX];
        int externalExit

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