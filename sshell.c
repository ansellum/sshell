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

int saved_stdout;
int changed_stdout = 0;
int oRedirectSymbolDetected = 0;

typedef struct commandObj {
        char* program;
        char* arguments[NUMARGS_MAX + 1]; // +1 for program name
        int numArgs;
        int order;
}commandObj;

/*Debug function*/
void printCommands(struct commandObj* cmd, int index)
{
        fprintf(stdout, "\n------------------------------\n");
        for (int i = 0; i <= index; ++i)
        {
                fprintf(stdout, "Program \t%d: %s\n", i, cmd[i].program);
                fprintf(stdout, "Arguments \t%d: ", i);
                for (int j = 1; j <= cmd[i].numArgs; ++j) printf("[%s] ", cmd[i].arguments[j]);
                fprintf(stdout, "\n");
        }
        fprintf(stdout, "------------------------------\n\n");
}

void printCommand(struct commandObj cmd)
{
        printf("------------------------------\n");
        printf("Program \t: %s\n", cmd.program);
        printf("Arguments \t: ");
        for (int j = 1; j <= cmd.numArgs; ++j) printf("[%s] ", cmd.arguments[j]);
        printf("\n");
        printf("------------------------------\n\n");
}

/*Redirect output*/
void redirectOutput(char *filename)
{
        int fd;
       
        //check if file exists & is allowed to be edited
        if(!access(filename, F_OK ) && !access(filename, W_OK))
        {
                fd = open(filename, O_WRONLY | O_TRUNC); //open file as write only & truncate contents if it's
                if (fd == -1) return; //open() is unsuccessful
                saved_stdout = dup(1);
                dup2(fd, 1);
                changed_stdout = 1;
                close(fd);
        }
        else fprintf(stderr, "Error: cannot open input file\n");
       
        return;
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
int parseCommand(const int index, char **filename, struct commandObj* cmd, char* cmdString)
{
        char delim[] = " ";
        char* token;
        char** copyofFileName = filename;
        int numPipes = index;
        int argIndex = 1;

        //make editable string from literal
        char* command1 = malloc(CMDLINE_MAX * sizeof(char));
        char* command2 = malloc(CMDLINE_MAX * sizeof(char));
        char* copyofCommand = malloc(CMDLINE_MAX * sizeof(char));
        strcpy(command1, cmdString);
        strcpy(copyofCommand, cmdString);
       
        /*Parse output redirection symbol*/
        command2 = command1;
        command1 = strsep(&command2, ">"); //command1 = before '>', command2 = after '>'
       
        if (command2 != NULL)
        {
                *filename = command2;
                while (*filename[0] == ' ') (*filename)++;
        }
        //command contains '>' character
        if (!(strchr(copyofCommand, '>') == NULL)) oRedirectSymbolDetected = 1;
       
        /*Recusively parse pipes*/
        command2 = command1;
        command1 = strsep(&command2, "|"); //command1 = first command, command2 = rest of the cmdline
        if(command2 != NULL) numPipes = parseCommand(index + 1, filename, cmd, command2);

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

void executePipeline(int fd[][2], int exitval[], struct commandObj* cmd, char* filename, int numPipes, const int index)
{
        int status;
        int pid = fork();

        if (pid == 0) { //Child process

                if (index > 0)          dup2(fd[index - 1][0], STDIN_FILENO);   //redirect stdin to read pipe,  unless it's the first command
                if (index < numPipes)   dup2(fd[index][1], STDOUT_FILENO);      //redirect stdout to write pipe, unless it's the last command
                else if (filename[0] != '\0')                                   //check if filename is detected, and execute redirection
                {
                    redirectOutput(filename);
                }
                //'>' symbol used, but no fileName provided
                else if (filename[0] == '\0' && oRedirectSymbolDetected) fprintf(stderr, "Error: no input file\n");

                //close all pipes
                for (int i = 0; i < numPipes; ++i)
                {
                        close(fd[i][0]);
                        close(fd[i][1]);
                }
                //execute program if we changed stdout successfully or a '>' isn't inputted
                if (changed_stdout || (filename[0] == '\0' && !oRedirectSymbolDetected) || index < numPipes) exitval[index] = execvp(cmd[index].program, cmd[index].arguments);
                else exit(1);
        }
        else if (pid > 0) { //Parent process
                if (index < numPipes) executePipeline(fd, exitval, cmd, filename, numPipes, index + 1);

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

 /*Executes an external command with fork(), exec(), & wait() (phase 1)*/
void prepareExternalProcess(char *cmdString)
{
        int numPipes, status;
        char* filename = malloc(ARGLENGTH_MAX * sizeof(char));
        commandObj cmd[PIPES_MAX];

        if (strlen(cmdString) == 0) return;

        /*Parse and check for errors*/
        filename[0] = '\0';     //For conditional in executePipeline
        numPipes = parseCommand(0, &filename, cmd, cmdString);
        if (numPipes < 0)
        {
                fprintf(stderr, "Error: too many process arguments\n");
                return;
        }

        /* Builtin commands*/
        if (!strcmp(cmd[0].program, "cd"))
        {
                status = changeDirectory(cmd[0].arguments);
                fprintf(stderr, "+ completed '%s' [%d]\n", cmdString, status);
                return;
        }
        else if (!strcmp(cmd[0].program, "pwd"))
        {
                status = printWorkingDirectory();
                fprintf(stderr, "+ completed '%s' [%d]\n", cmdString, status);
                return;
        }
        else if (!strcmp(cmd[0].program, "exit"))
        {
                fprintf(stderr, "Bye...\n");
                fprintf(stderr, "+ completed '%s' [%d]\n", cmdString, EXIT_SUCCESS);
                exit(EXIT_SUCCESS);
        }

        /*Piping (works with single commands)*/
        int fd[numPipes][2];
        int exitval[numPipes + 1];

        for (int i = 0; i < numPipes; ++i)
        {
                if (pipe(fd[i]) != 0)
                        exit(EXIT_FAILURE);
        }
        executePipeline(fd, exitval, cmd, filename, numPipes, 0);
       
        //restore detectors
        if (changed_stdout)
        {
                dup2(saved_stdout, 1);
                close(saved_stdout);
                changed_stdout = 0;
        }
        if (oRedirectSymbolDetected) oRedirectSymbolDetected = 0;

        fprintf(stderr, "+ completed '%s' ", cmdString);
        for (int i = 0; i < numPipes + 1; ++i) fprintf(stderr, "[%d]", exitval[i]);  //Print exit values
        fprintf(stderr, "\n");

        //Debug command objects
        //printCommands(cmd, numObjects);

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