# Project #1 Design

## Main Components

### 1) Prepare External Process (The Decision Maker)

- Overview: Calls the two subsequent main components and determines the high level flow of our program. 

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