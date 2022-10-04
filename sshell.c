#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define CMDLINE_MAX 512

typedef struct commandObj {
        char* program;
        char arguments[16][32];    //Array of 16 strings (0-15) that holds 32 characters each
}commandObj;

void parseCommand(struct commandObj* cmd, char *cmdString)
{
        int index = 0;
        //get the first token and stuff it in the program property 

        char *token = strtok(cmdString, " ");
        cmd->program = token;
        //pass the arguments into the arguments array
        while (token != NULL) {
                //update token to next argument
                token = strtok(NULL, " ");

                strcpy(cmd->arguments[index], token);

                index++;

        }
}

 /* Executes an external command with fork(), exec(), & wait() (phase 1)*/
void executeExternalProcess(char *cmdString) 
{
        int pid;
        int childStatus;
        commandObj cmd;

        pid = fork();
        //child process should execute the command (takes no arguments yet)
        if (pid == 0) {
                parseCommand(&cmd, cmdString);
                childStatus = execlp(cmdString, cmdString, (char *) NULL);
                exit(1); //if child reaches this line it means there was an issue running command
        }
        //parent process should wait for child to execute
        else if (pid > 0) {
                waitpid(pid, &childStatus, 0);
                printf("+ completed '%s' [%d]\n", cmdString, childStatus);
        }
        //handle error with fork()
        else fprintf(stderr, "error completing fork() [%d]\n", 1);

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

                /* Builtin command */
                if (!strcmp(cmdString, "exit")) {
                        fprintf(stderr, "Bye...\n");
                        fprintf(stderr, "+ completed '%s' [%d]\n",cmdString, 0);
                        break;
                }
                else executeExternalProcess(cmdString);
        }
        
        return EXIT_SUCCESS;
}
