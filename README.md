Operating Systems – Milestone Overview
In this milestone, you are asked to create your own scheduler and manage the usage of the resources between different processes. You are provided with three program files, each representing a separate program.
Your tasks include:

Creating an interpreter to read and execute the .txt files.

Implementing memory management to store processes.

Implementing mutexes to ensure mutual exclusion over critical resources.

Implementing a scheduler to manage process execution.

Detailed Description
Programs
There are 3 main programs:

Program 1: Given two numbers, prints the numbers between them (inclusive).

Program 2: Given a filename and data, writes the data to the file. The file is always newly created.

Program 3: Given a filename, prints the contents of the file on the screen.

Process Control Block (PCB)
A PCB is a data structure used by operating systems to store information about a process. Each process should have a PCB containing:

Process ID (assigned upon creation)

Process State

Current Priority

Program Counter

Memory Boundaries (lower and upper bounds of the process’s memory space)

Program Syntax
Each instruction takes one clock cycle to execute. Syntax is as follows:

print x — Prints variable x to the screen.

assign x y — Assigns value y to variable x. y may be:

A literal (e.g., number, string), or

The keyword input which prompts: “Please enter a value” and waits for user input.

writeFile x y — Writes value y to file x.

readFile x — Reads contents of file x.

printFromTo x y — Prints all numbers between x and y (inclusive).

semWait x — Acquires the mutex for resource x.

semSignal x — Releases the mutex for resource x.

Memory
The memory consists of 60 words, enough to store:

Unparsed lines of code

Variables

PCB information

Each memory word holds a name and its corresponding value (e.g., State: "Ready").
Memory allocation occurs at process arrival time, and each process requires space for:

Its program instructions

3 variables

Its PCB

Memory segments can be separated as needed, as long as they reside within the same memory structure.

Scheduler
A scheduler handles the execution order of processes in the Ready Queue. You are required to implement the following algorithms:

First Come First Serve (FCFS)

Round Robin with configurable quantum (input from user)

Multilevel Feedback Queue (MLFQ):

4 priority levels (1 is highest, 4 is lowest)

Quantum starts at 1 and doubles for each level

Level 4 uses Round Robin

You must schedule processes using these custom implementations.

Mutual Exclusion
You are required to implement mutexes for:

File access (read/write)

User input

User output

Mutexes use:

semWait <resource> to request access

semSignal <resource> to release access

For example:

plaintext
Copy
Edit
semWait userOutput
print x
semSignal userOutput
Rules:
Only one process may access a resource at a time.

If the resource is in use, the requesting process is:

Blocked, and

Added to both the resource’s blocked queue and the general blocked queue.

Unblocking is based on priority: the highest-priority blocked process gets unblocked first.

Once unblocked, a process goes to the appropriate ready queue based on its priority.

GUI Requirements for the Scheduler Simulation
The GUI must allow users to interact with the OS simulation by viewing and controlling processes, memory, and resources.

1. Main Dashboard
Overview Section:

Total number of processes

Current clock cycle

Active scheduling algorithm

Process List:

Process ID

State (Ready, Running, Blocked)

Current Priority

Memory Boundaries

Program Counter

Queue Section:

Ready Queue

Blocked Queue

Running Process

Current instruction

Time in queue

2. Scheduler Control Panel
Scheduling Algorithm Selector (dropdown):

FCFS

Round Robin (user-defined quantum)

Multilevel Feedback Queue

Buttons:

Start: Begins the simulation

Stop: Pauses the simulation

Reset: Clears all memory and processes

Quantum Input: User can set Round Robin quantum

3. Resource Management Panel
Mutex Status:

Displays which process holds or is waiting for:

userInput

userOutput

file

Blocked Queue Viewer:

Lists all waiting processes and their priorities

4. Memory Viewer
Displays allocation of the 60 memory words as a grid or list

5. Log & Console Panel
Execution Log:

Real-time log of instructions

Shows process ID and system response

Event Messages:

Messages like process blocking, unblocking, etc.

6. Process Creation and Configuration
Add Process:

Button to upload a .txt program file

Set Arrival Time:

Users define when each process arrives (before simulation starts)

7. Visualized Execution
Step-by-Step Execution: Advances simulation by one clock cycle

Auto Execution: Runs simulation continuously until completion or manual stop

Additional Notes
Process scheduling order is user-configurable

Memory should be shown clearly each clock cycle

Real-time updates are required to:

Track currently executing process

Show resource statuses

Reflect memory allocation changes
