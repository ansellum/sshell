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

/* Parses command into commandObj properties; returns number of arguments */
int parseCommand(struct commandObj* cmd, char *cmdString)
{
        char delim[] = " ";
        char* token;
        int i = 1;

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
                cmd->arguments[i] = malloc(ARGLENGTH_MAX * sizeof(char));
                strcpy(cmd->arguments[i], token);

                i++;
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