#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define CMDLINE_MAX 512
#define NUMARGS_MAX 16
#define ARGLENGTH_MAX 32

typedef struct commandObj {
        char* program;
        char* arguments[NUMARGS_MAX];
}commandObj;

/* Parses command into commandObj properties */
void parseCommand(struct commandObj* cmd, char *cmdString)
{
        char delim[] = " ";
        char* quotes = "\'\"";
        char* token;

        //make editable string from literal
        char* buf = malloc(CMDLINE_MAX * sizeof(char));
        strcpy(buf, cmdString); 

        //pass program name into command object
        token = strsep(&buf, delim);
        cmd->program = token;
        cmd->arguments[0] = token;

        //parse the rest of the command
        int i = 1;
        while (buf != NULL && strlen(buf) != 0) {
                /* Check for special cases first */
                while (buf[0] == ' ') token = strsep(&buf, delim); //Extra spaces
                if (buf[0] == '\'' || buf[0] == '\"') //Quotations
                {
                        buf++;
                        token = strsep(&buf, quotes);
                }
                else token = strsep(&buf, delim);

                //pass token into command arguments
                if (token == NULL) break;
                cmd->arguments[i] = malloc(ARGLENGTH_MAX * sizeof(char));
                strcpy(cmd->arguments[i], token);
                i++;
        }
        //End arguments array with NULL for execvp() detection
        cmd->arguments[i] = NULL;
}

 /* Executes an external command with fork(), exec(), & wait() (phase 1)*/
void executeExternalProcess(char *cmdString) 
{
        int pid;
        int childStatus;
        commandObj cmd;

        parseCommand(&cmd, cmdString);

        pid = fork();
        //child process should execute the command (takes no arguments yet)
        if (pid == 0) {
                childStatus = execvp(cmd.program, cmd.arguments);
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
