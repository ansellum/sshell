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

int oRedirectSymbolDetected = 0;
int iRedirectSymbolDetected = 0;
char* oFile;
char* iFile;

typedef struct commandObj {
        char* program;
        char* arguments[NUMARGS_MAX + 1]; // +1 for program name
        int numArgs;
}commandObj;

/*Redirect*/
void redirectOutput(int fd)
{
        fd = open(oFile, O_CREAT | O_WRONLY | O_TRUNC); //open file as write only & truncate contents if it's
        if (fd == -1) return; //open() is unsuccessful
        dup2(fd, STDOUT_FILENO);
}

void redirectInput(int fd)
{
        fd = open(iFile, O_RDONLY); //open file as read only
        if (fd == -1) return; //open() is unsuccessful
        dup2(fd, STDIN_FILENO);
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
                exitval[index] = execvp(cmd[index].program, cmd[index].arguments);
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
        exitval[index] = WEXITSTATUS(status);
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
       
        /*Parse output redirection symbol*/
        command2 = command1;
        command1 = strsep(&command2, "<"); //command1 = before '>', command2 = after '>'
        if (command2 != NULL)
        {
                iRedirectSymbolDetected = 1;
                iFile = command2;
                while (iFile[0] == ' ') iFile++;
        }

        /*Parse output redirection symbol*/
        command2 = command1;
        command1 = strsep(&command2, ">"); //command1 = before '>', command2 = after '>'
        if (command2 != NULL)
        {
                oRedirectSymbolDetected = 1;
                oFile = command2;
                while (oFile[0] == ' ') oFile++;
        }
       
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

        //iterate through arguments
        while ( command1 != NULL && strlen(command1) != 0) {
                /*Error checking*/
                if (argIndex >= NUMARGS_MAX) return -1;   //Too many arguments

                //skip extra spaces
                while (command1[0] == ' ') command1++;

                //update token
                token = strsep(&command1, delim);

                //pass command arguments
                if (strlen(token) == 0) break;
                cmd[index].arguments[argIndex] = malloc(ARGLENGTH_MAX * sizeof(char));
                strcpy(cmd[index].arguments[argIndex], token);
                cmd[index].numArgs = argIndex++;
        }
        //End arguments arr with NULL for execvp() detection
        cmd[index].arguments[argIndex] = NULL;

        return numPipes;
}

void builtin_commands(struct commandObj cmd, char* cmdString)
{
        int status;
        if (!strcmp(cmd.program, "cd"))
        {
                status = changeDirectory(cmd.arguments);
                fprintf(stderr, "+ completed '%s' [%d]\n", cmdString, status);
                return;
        }
        else if (!strcmp(cmd.program, "pwd"))
        {
                status = printWorkingDirectory();
                fprintf(stderr, "+ completed '%s' [%d]\n", cmdString, status);
                return;
        }
        else if (!strcmp(cmd.program, "exit"))
        {
                fprintf(stderr, "Bye...\n");
                fprintf(stderr, "+ completed '%s' [%d]\n", cmdString, EXIT_SUCCESS);
                exit(EXIT_SUCCESS);
        }
}

 /*Executes an external command with fork(), exec(), & wait() (phase 1)*/
void prepareExternalProcess(char *cmdString)
{
        int numPipes;
        oFile = malloc(ARGLENGTH_MAX * sizeof(char));
        commandObj cmd[PIPES_MAX];

        if (strlen(cmdString) == 0) return;

        /*Parse and check for errors*/
        oFile[0] = '\0';     //For conditional in executePipeline
        numPipes = parseCommand(0, cmd, cmdString);
        if (numPipes < 0)
        {
                fprintf(stderr, "Error: too many process arguments\n");
                return;
        }
        if (strlen(oFile) == 0 && oRedirectSymbolDetected)
        {
                fprintf(stderr, "Error: no output file\n");
                return;
        }
        if (strlen(iFile) == 0 && iRedirectSymbolDetected)
        {
                fprintf(stderr, "Error: no input file\n");
                return;
        }

        /* Builtin commands*/
        builtin_commands(cmd[0], cmdString);

        /*Piping (works with single commands)*/
        int fd[numPipes][2];
        int exitval[numPipes + 1];

        for (int i = 0; i < numPipes; ++i)
        {
                if (pipe(fd[i]) != 0)
                        exit(EXIT_FAILURE);
        }
        executePipeline(fd, exitval, cmd, numPipes, 0);
       
        //restore detectors
        oRedirectSymbolDetected = 0;
        iRedirectSymbolDetected = 0;

        fprintf(stderr, "+ completed '%s' ", cmdString);
        for (int i = 0; i < numPipes + 1; ++i) fprintf(stderr, "[%d]", exitval[i]);  //Print exit values
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