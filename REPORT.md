# Introduction

The goal of this project is to understand important UNIX system calls by  
developing a stripped-down UNIX shell named **sshell**. This version sports the  
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
(i.e. not the entire command line). This includes the program name, an array of  
arguments, and the total number of arguments.  

This struct was designed specifically for *sshell*.

### 2) StringStack (stringstack.c)
This structure represents a stack of string variables with a max string length  
of PATH_MAX (4096) characters. It utilizes a constructor function which defines  
the stack's upper item limit and two functions (`push()` and `pop()`) that help  
the user interact with the stack.

This struct was designed to be a general standalone stack for strings. *sshell*  
expands on its  `push()` and `pop()` functions for use with directory names  
(more on this below).

### Potential future modifications
There was a plan to consolidate certain data types in a "Command Line" struct,  
which would've helped organize properties of the **entire command line**. Due to  
time constraints, this feature was not implemented in the final release.

# Program Flow (Functions)

## Main Function

This is where the program begins. The main function remains largely unchanged  
from the skeletron provided at `/home/cs150jp/public/p1/sshell.c` on the CSIF  
systems.  

The most important parts of this function are to
1) capture the user Input  
2) create a directory stack using the custom StringStack data type, and
3) execute a while statement that calls prepareExternalFunction()` every loop
   and runs until the program receives a signal to stop.

## Prepare External Process (The Controller)

`prepareExternalProcess()` is exactly what it sounds like - it prepares *sshell*  
for forking and execution of the command line. In order to reach this point,  
`prepareExternalProcess()` iterates through a number of functions, each with a  
specific purpose. As a result, `prepareExternalProcess()`'s flow is structured  
something like this:

1. Call parsing functions (`parseCommand()`)
2. Check for parsing errors (`errorManagement()`)
3. Check (& execute) special commands (`specialCommands()`)
4. Prepare pipes and begin executing commands (`executePipeline()`)
5. Reset global variables
6. Output completion message
 
## Parse Command (The Analyst)

`parseCommand()` is by far the largest function in *sshell* in terms of  
functionality. It sports two different sections; one for parsing delimiters  
( '<', '>', '|' ) and the other for parsing arguments. Delimiters are parsed  
*before* arguments as to avoid confusing filenames with arguments.  

`parseCommand()` utilizes recursive functionality to handle pipes, calling  
itself iteratively until there are no more `|` symbols detected. This is  
performed using a return value which increments once per pipe, which will later  
be passed to `executePipeline()`. 

This return value has a second functionality in error management, returning a  
unique negative error code for each type of parsing error.

## Error Management (The IT guy)
`errorManagement()` deciphers the exit code from `parseCommand()` using a switch  
statement. If one of the error codes match, the function will output the  
associated error and cause *sshell* to start a new command line.

## Special Commands (The Specialist)
`specialCommands()` checks the first (leftmost) commmandObj's program name  
against commands that require special attention. These are a combination of  
built-in commands (`cd`, `pwd`, `exit`) and directory stack commands.

## Execute Pipeline (The Action Taker)

- Overview: If pipes have been used, sets up pipeline and calls execvp() to execute commands with the data collected during parsing.

- Core Responsibilities:

    - call fork()
    - setting up pipeline if pipes have been inputted
    - call execvp() to execute external commands
    - close all pipes and collect exit stauses

- Note: recursively calls itself when pipes are detected for executing each individual command in the pipeline