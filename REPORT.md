# Project #1 Design

## Main Components

### 1) Prepare External Process (The Decision Maker)

-  This component is responsible for calling the two subsequent main components and determines the high level flow of our program. 

- Responsibilities:

    - call parsing funciton
    - complete high-level error checking
    - call builtin commands function
    - redirect file descriptors if pipes have been detected
    - call execute pipeline function
    - output completion message
 
### 2) Parse Command (The Analyst)

### 3) Execute Pipeline (The Grunt Man)