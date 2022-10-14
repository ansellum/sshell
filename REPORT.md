# SSHELL: a stripped UNIX sshell

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

## Implementation
The implementation of this program follows five distinct steps:

1. Parse the user input using a command struct for organization
2. Error Management
3. Special Commands
4. Execute user input
5. Repeat

### Parse User Input

At the beginning of its execution, `sshell` will capture user input and build a  
*StringStack* data structure which will be used for the directory stack. The  
user input is immedietely parsed using two phases, the first for delimiters  
(`parseDelimiter()`) and second for a **single command** (`parseCommand()`).

   The first phase parses delimiters for I/O redirection and piping, returning  
   if a parsing error is detected. This is done returning unique negative values  
   which indicate the type of parsing error. If this phase detects a `>` or `<`  
   character in the command line, it will note the file's existence using global  
   variables. If this phase detects a `|` character, it will recursively call  
   `parseDelimiter()`, passing the string that was beyond the pipe character,
   until there are no more pipe characters detected. This creates multiple  
   layers of `parseDelimiter()`, each with a single command that passes to  
   `parseCommand()'.  

   The second phase of the parse takes the command string passed by  
   `parseDelimiter()` and parses it into a *commandObj* data structure. Parsing  
   is accomplished using `strsep()` to separate the command string and a while  
   loop to iteratively stores each argument in the data structure.  

### Error Management

If there is a parsing error from the parse step, `numPipes` will be set to a  
unique negative value. This value is passed to `errorManagement()`, which will  
decipher the error and return from execution (i.e. trash the command line and  
reprompt).

### Special Commands

With errors checked, `sshell` will then check if a command was input that  
requires special attention. This includes the built-in commands (`cd`, `pwd`,  
 `exit`) and the commands used for the directory stack.  
 
   `cd` and `pwd` use C library commands that change the directory and print the  
   current working directory respectively.  
   `exit` will call the `exit()` C command with EXIT_SUCCESS.  
   
   `dirs` uses the pre-existing `pwd` function and iterates through the  
   *StringStack* items, printing them from last to first. This correlates to  
   printing the stored directories from top to bottom within the *StringStack*.  
   `pushd` first pushes the current working directory to the stack using `pop()`,  
   then changes the directory.  
   `popd` must use `pop()` before changing directory. This is because the `pop()`  
   function checks if an empty 

























