#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define CMDLINE_MAX 512

struct commandObj {
        char* program;
        char* arguments[16][32];    //Array of 16 strings (0-15) that holds 32 characters each
};

parseCommand(struct commandObj* obj, char *command) {
        const char delimiter[2] = " ";
        char* token;
        int index = 0;

        //get the first token and stuff it in the program property 
        char* token = strtok(command, s);
        obj->program = token;

        //pass the arguments into the arguments array
        while (token != NULL) {
                obj->arguments[index] = token;

                //update token to next argument
                token = strtok(NULL, delimiter);

                index++;
        }
}

 /* Executes an external command with fork(), exec(), & wait() (phase 1)*/
void executeExternalProcess(char *cmd) 
{
        int pid;
        int childStatus;

        pid = fork();
        //child process should execute the command (takes no arguments yet)
        if (pid == 0) {
                execlp(cmd, cmd, (char *) NULL);
                exit(1); //if child reaches this line it means there was an issue running command
        }
        //parent process should wait for child to execute
        else if (pid > 0) {
                wait(&childStatus);
                //print error status of child process to stderr
                if (!WEXITSTATUS(childStatus)) fprintf(stderr, "+ completed '%s' [%d]\n",cmd, 0); //if child process is successful
                else fprintf(stderr, "+ completed '%s' [%d]\n",cmd, 1);
        }
        //handle error with fork()
        else fprintf(stderr, "error completing fork() [%d]\n", 1);

        return;
}

int main(void)
{
        char cmd[CMDLINE_MAX];

        while (1) {
                char *nl;
        
                /* Print prompt */
                printf("sshell@ucd$ ");
                fflush(stdout);

                /* Get command line */
                fgets(cmd, CMDLINE_MAX, stdin);

                /* Print command line if stdin is not provided by terminal */
                if (!isatty(STDIN_FILENO)) {
                        printf("%s", cmd);
                        fflush(stdout);
                }

                /* Remove trailing newline from command line */
                nl = strchr(cmd, '\n');
                if (nl) *nl = '\0';

                /* Builtin command */
                if (!strcmp(cmd, "exit")) {
                        fprintf(stderr, "Bye...\n");
                        fprintf(stderr, "+ completed '%s' [%d]\n",cmd, 0);
                        break;
                }
                else executeExternalProcess(cmd);
        }
        
        return EXIT_SUCCESS;
}
