# Project #1

## Introduction

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

## Structures

*sshell* uses two custom structs that help organize data within the program.  

### CommandObj
###### sshell.c
This structure holds the necessary properties for a executing a **single command**  
(i.e. not the entire command line). This includes the program name, an array of  
arguments, and the total number of arguments.  

This struct was designed specifically for *sshell*.

### StringStack
This structure represents a stack of string variables with 

## Program Flow (Functions)

### Main Function

This is where the program begins. The main function remains largely unchanged  
from the skeletron provided at `/home/cs150jp/public/p1/sshell.c` on the CSIF  
systems.  

The most important part of this function is the while statement that  
calls `prepareExternalFunction()` and runs until the program receives a signal  
to stop.

### 1) Prepare External Process
###### The Controller

Calls the two subsequent main components and determines the high level flow of our program. 

- Core Responsibilities:

    - call parsing funciton
    - complete high-level error checking
    - call builtin commands function
    - call execute pipeline function
    - output completion message
 
### 2) Parse Command (The Analyst)

- Overview: Parses through the full command string input, splits it up, and stores the data needed later.

- Core Responsibilities:

    - detect if delimiters have been inputted ( "<" , ">" , or "|") and handle appropriately
    - store program names ('date', 'echo', ect.)
    - verify the number of arguments inputted is within the limit and ignore extraneous spaces
    - store arguments for programs

- Note: function recursively calls itself if pipes are detected in order to store information for each individual command in pipeline

### 3) Execute Pipeline (The Action Taker)

- Overview: If pipes have been used, sets up pipeline and calls execvp() to execute commands with the data collected during parsing.

- Core Responsibilities:

    - call fork()
    - setting up pipeline if pipes have been inputted
    - call execvp() to execute external commands
    - close all pipes and collect exit stauses

- Note: recursively calls itself when pipes are detected for executing each individual command in the pipeline