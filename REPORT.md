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
2. Check for errors
3. Check for special commands
4. Execute user input
5. Repeat

### Parse User Input

At the  beginning of its execution, `sshell` will capture user input and build a  
StringStack data structure which will be used for the directory stack feature. This  
StringStack structure is a single object containing stack properties and two  
functions which help interact with the stack (`push()` and `pop()`). The user  
input is immedietely parsed using two phases, one for delimiters  
(`parseDelimiter()`) and one for a **single command** (`parseCommand()`).

The first phase parses delimiters for I/O redirection and piping, skipping  
command execution if a parsing error is detected. This is done by utilizing  
unique return values which indicate the type of parsing error. If this phase  
detects a `>` or  `<` character in the command line, it will store the name and  
existence of the file using global variables. If this phase detects a `|`  
character, it will call `parseDelimiter()` again, passing a substring containing  
everything that was beyond the pipe character. This substring will be checked  
for the delimiters again, and this process will recurse until no more pipes are  
found.

The second phase of the parse takes a command string with no delimiters (`<`,`>`,
`|`) and seperates by the spaces. It does this by utilizing `strsep()`, which  
allows a while loop to iteratively chop up the command string  
and store them in the correct property.
array relies on `parseDelimiter()`'s recursive  
characteristic; in fact, `parseCommand()` is called at the end of  
`parseDelimiter()` (The seperation is to enforce single-purpose functions).