# SSHELL: a basic UNIX sshell

## Summary
The goal of this project is to understand important UNIX system calls by  
developing a barebones UNIX shell named **sshell**. This version sports the  
following features:

- Built-in UNIX commands w/ argument support (e.g. *date*, *ls*, *cd*)
- Standard input & output redirection
- Command piping (*up to three*)
- Directory stack
- Comprehensive error management

The main design considerations of sshell include a focus on single-purpose  
statements, readability, and function separation.

## Data Organization

*sshell* uses two custom structs that help organize data within the program.  

### 1) CommandObj (sshell.c)
This structure holds the necessary properties for a executing a **single command**  
(i.e. not the entire command line). 

``` 
typedef struct commandObj
{
        char* program;
        char* arguments[NUMARGS_MAX + 1]; // +1 for program name
        int numArgs;
}commandObj;
```

This struct was designed specifically for *sshell*.

### 2) StringStack (stringstack.c)
This structure represents a stack of string variables with a max string length  
of PATH_MAX (4096) characters. It utilizes a constructor function which defines  
the stack's upper item limit and two functions (`push()` and `pop()`) that help  
the user interact with the stack.

```
typedef struct stringStack
{
        int maxSize;
        int top;
        char** items;
}stringStack;
```

This struct was designed to be a general standalone stack for strings. *sshell*  
expands on its  `push()` and `pop()` functions for use with directory names  
(more on this below).

## Implementation
The implementation of this program follows five distinct steps:

1. Parse the user input using a command struct for organization
2. Error Management
3. Special Commands
4. Execute user input
5. Repeat

### Parse User Input

At the beginning of its execution, `sshell` will capture user input and build an  
array of *commandObj* that supports up to three pipes. The user  
input is passed through two phases, the first for parsing delimiters  
(`parseDelimiter()`) and second for parsing **single commands** (`parseCommand()`).

#### First phase
The first phase, `parseDelimiters()`, parses delimiters for I/O redirection and  
piping, returning if a parsing error is detected. This is done by checking for  
every possible case and returning unique negative values which indicate the type  
of parsing error. 

If `parseDelimiter()` detects a delimiter, it will seperarate the string before  
and after the delimiter into substrings. Assuming there are no errors, if the  
detected delimiter was `>` or `<`, it will note the file's existence  
using global variables. If `parseDelimiter()` detects a `|` character, it will  
recursively call itself, passing the string that was beyond the pipe character,  
until there are no more pipe characters detected. This creates multiple layers  
of `parseDelimiter()`, each passing a single command to `parseCommand()'.  

```
int parseDelimiters(const int index, commandObj* cmd, char* cmdString)
{
	...
	numPipes = parseDelimiters(index + 1, cmd, afterDelim);
	...
	parseCommand(index, cmd, beforeDelim);
	return numPipes;
}
```

#### Second phase
The second phase of the parse takes the command string passed by  
`parseDelimiter()` and parses it into their respective *commandObj* data  
structure. Parsing is accomplished using `strsep()` to separate the command  
string by the spaces. A while loop then iteratively stores each argument in the  
correct data structure property.  

### Error Management

If there is a parsing error from the previous step, `numPipes` will be set to a  
unique negative value. This value is passed to `errorManagement()`, which will  
print the error and return from execution (i.e. trash the command line and  
reprompt).

### Special Commands

With errors checked, `sshell` will then check if a command was input that  
requires special attention. This includes the built-in commands (`cd`, `pwd`,   
`exit`) and the commands used for the directory stack.  
 
`cd` calls the `changeDirectory()` function, which calls the C function `chdir()`.  
`pwd` calls the `printWorkingDirectory()` function, which calls the C function  
`getcwd()`. 
`exit` will call the C function `exit()` with argument EXIT_SUCCESS.  
   
`dirs` first prints the current working directory by calling  
`printWorkingDirectory()`. It then iterates through directoryStack's items, if  
any. These iterations traverse from the end of the item array to the front,  
which correlates to printing the stored directories from top to bottom within  
the *StringStack*.  
`pushd` first pushes the current working directory to the stack using  
*StringStack*'s `push()` function, then changes the directory using the  
`changeDirectory()` function.  
`popd` uses must use `pop()` before changing directories. This so that `sshell`  
can check if the *StringStack* is empty before trying to change directories.

If a special command is detected and run, `sshell` will skip the next step.

### Execute Commands  
`sshell` first makes an array of pipe file descriptors, passing them to  
`executePipeline()`, along with an integer `index` = 0. `executePipeline()`  
begins by using `fork()` to create a parent and child process, each running  
different code that is seperated by comparing their PIDs.

#### Parent Process  
The parent process first checks if there are multiple commands by comparing the  
current command's index to the number of pipes. If there are multiple commands  
(i.e. a pipe exists), it will start a recursive call to `executePipeline()`.  
This call will fork another child process and then check for another command.  
Once the necessary amount of child processes are forked, the parent will `wait()`  
for the death of all of its children before continuing. 

```
executePipeline() {
	pid = fork();
	...
	if (pid > 0) { //Parent process
		if (index < numPipes) executePipeline();
		...
	}
	waitpid(pid, &status, 0);
}
```

#### Child Process  
A single child process prepares and executes the *commandObj* that its `index`  
correlates to. For example, the first child will run the first command, the  
second child will run the second command, etc. First, the child compares  
`index`'s value to 0 and the number of pipes in order to determine its order in  
the command line. If it is first or last, it only redirects its input or output  
if the global variable `oRedirectSymbolDetected` or `iRedirectSymbolDetected` is  
set, respectively. Commands in between other commands will have both their file  
descriptors redirected. 

Redirection takes place in the `redirectOutput()` and `redirectInput()` functions,  
respectively. These functions utilize the C library function `dup2()` to replace  
the standard input/output with the requested file descriptor.

After redirecting the file descriptors, the child process executes the command  
using `execvp()`. If the child process runs past this, `execvp()` failed and the  
child process prints an error to `stderr`.

```
executePipeline()
{
	int status;
	int pid = fork();

	if (pid == 0) { //Child process

		if (index > 0)                   dup2(fd[index - 1][0], STDIN_FILENO);
		else if (inputRedirectDetected)  redirectInput(fd[index][0]);

		if (index < numPipes)            dup2(fd[index][1], STDOUT_FILENO);
		else if (outputRedirectDetected) redirectOutput(fd[index][1]);
		
		...
		
		execvp(command[index], arguments);
	}
}
```

#### Note about Recursion
The idea behind using recursion for execution was to iteratively fork enough  
child processes in order to run each command. Each of these child processes runs  
concurrently with one another, since child processes never call to `wait()`. It  
is only the parent that calls `wait()`, and only *after* it forks all required  
child processes.

### Repeat

`sshell`'s main function is comprised of a while loop that repeat this process.  
The while loop first prints `sshell@ucd$ ` and then waits for user input. After  
capturing it, it then passes it and an empty directory stack to  
`prepareExternalProcess()` for use. `prepareExternalProcess()` then performs  
the  above actions through the described functions and in the described order.  
Repeat ad infinitum until `sshell` receives a signal that causes it to terminate.



















