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
}commandObj;

void printCommands(struct commandObj* cmd, int index)
{
        for (int i = 0; i <= index; ++i)
        {
                printf("Program %d:\t%s\n", i, cmd[i].program);
                printf("Arguments %d:\t", i);
                for (int j = 1; j <= cmd[i].numArgs; ++j) printf("[%s] ", cmd[i].arguments[j]);
                printf("\n");
        }
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
void printWorkingDirectory() 
{
        char cwd[CMDLINE_MAX];

        getcwd(cwd, sizeof(cwd));
        printf("%s\n", cwd);
        return;
}

/* Parses command into commandObj properties. Returns # of cmd objects, -1 if too many arguments in one command*/
int parseCommand(int* highestCMDIndex, struct commandObj* cmd, char* cmdString)
{
        char delim[] = " ";
        char* token;

        const int index = *highestCMDIndex;
        int argIndex = 1;

        //make editable string from literal
        char* command1 = malloc(CMDLINE_MAX * sizeof(char));
        char* command2 = malloc(CMDLINE_MAX * sizeof(char));
        strcpy(command1, cmdString);

        /*Recusively parse pipes*/
        command2 = command1;
        command1 = strsep(&command2, "|");
        if (command2 != NULL) 
        {
                //CHECKPOINT: command1 = first command, command2 = rest of the cmdline
                ++(*highestCMDIndex);
                parseCommand(highestCMDIndex, cmd, command2);
        }

        /*Define command struct properties*/
        while (command1[0] == ' ') command1++;
        token = strsep(&command1, delim);
        cmd[index].program = token;
        cmd[index].arguments[0] = token;
        cmd[index].numArgs = 0;

        //iterate through arguments
        while ( command1 != NULL && strlen(command1) != 0) {
                /*Error checking*/
                if (argIndex > NUMARGS_MAX) return -1;   //Too many arguments

                //skip extra spaces
                while (command1[0] == ' ') token = strsep(&command1, delim);

                //update token
                token = strsep(&command1, delim);


                //pass command arguments
                cmd[index].arguments[argIndex] = malloc(ARGLENGTH_MAX * sizeof(char));
                strcpy(cmd[index].arguments[argIndex], token);
                cmd[index].numArgs = argIndex++;
        }
        //End arguments array with NULL for execvp() detection
        cmd[index].arguments[argIndex] = NULL;

        return 0;
}

 /* Executes an external command with fork(), exec(), & wait() (phase 1)*/
void executeExternalProcess(char *cmdString)
{
        int pid, childStatus, highestCMDIndex = 0;
        commandObj cmd[PIPES_MAX];

        //check number of arguments & run parse
        if (parseCommand(&highestCMDIndex, cmd, cmdString) < 0)
        {
                fprintf(stderr, "Error: too many process arguments\n");
                return;
        }
        printCommands(cmd, highestCMDIndex);

        //check if builtin command "cd" is called, utilizes parseCommand functionality
        if (!strcmp(cmd[0].program, "cd")) {
                int status = changeDirectory(cmd[0].arguments);
                fprintf(stderr, "+ completed '%s' [%d]\n", cmdString, status);
                return;
        }

        pid = fork();
        //child process should execute the command
        if (pid == 0) {
                childStatus = execvp(cmd[0].program, cmd[0].arguments);
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