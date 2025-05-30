# Operating-Systems

## Milestone Overview

In this milestone, you are asked to create your own scheduler and manage the usage of resources between different processes. You are provided with **three program files**, each representing a program. You are asked to create an interpreter that reads the `.txt` files and executes their code. You will also implement memory management to store the processes, and mutexes to ensure mutual exclusion over critical resources. Finally, you must implement a scheduler to manage process execution.

---

## Detailed Description

### Programs

We have **3 main programs**:

- **Program 1:** Given two numbers, prints the numbers between the two (inclusive).
- **Program 2:** Given a filename and data, writes the data to the file. Assume the file doesn't exist and should always be created.
- **Program 3:** Given a filename, prints the contents of the file on the screen.

---

### Process Control Block (PCB)

A PCB is a data structure used by operating systems to store all information about a process. You must keep a PCB for every process, with the following information:

- **Process ID:** Assigned upon creation.
- **Process State**
- **Current Priority**
- **Program Counter**
- **Memory Boundaries:** Lower and upper bounds of the process’ space in memory.

---

### Program Syntax

For the programs, use the following instructions:

- `print x` - Print the value of `x`.
- `assign x y` - Assign value `y` to variable `x`. `y` can be an integer, string, or `input`. If `y` is `input`, print "Please enter a value" and accept user input.
- `writeFile x y` - Write data `y` to file `x`.
- `readFile x` - Read and print contents of file `x`.
- `printFromTo x y` - Print all numbers between `x` and `y` (inclusive).
- `semWait x` - Acquire resource `x` (see Mutual Exclusion).
- `semSignal x` - Release resource `x` (see Mutual Exclusion).

**Note:**  
- Every line of instruction is executed in 1 clock cycle.

---

### Memory

- The memory is of fixed size: **60 words**.
- It stores unparsed code lines, variables, and PCB for any process.
- Each word can store a name and its corresponding data (e.g., `State: "Ready"`).
- Each process, upon creation (arrival time), is allocated space for instructions, variables (enough for 3 variables), and PCB.
- You can separate code, variables, and PCB within the same memory structure as long as they remain contiguous per process.

---

### Scheduler

The scheduler manages processes in the Ready Queue, ensuring all get a chance to execute. You need to implement:

1. **First Come First Serve (FCFS)**
2. **Round Robin (RR):** User can set the quantum.
3. **Multilevel Feedback Queue (MLFQ):**
    - Four priority levels (1 = highest, 4 = lowest).
    - Quantum doubles as you move to lower levels.
    - Last level uses Round Robin policy.

Your task: **Implement all three algorithms from scratch and use them to schedule processes.**

---

### Mutual Exclusion

A mutex is used to control access to shared resources with two atomic operations: `semWait` and `semSignal`. You must implement three mutexes, one for each resource:

- **File access** (read/write)
- **User input**
- **Screen output**

When using a mutex, pass the resource name:
- `userInput` (for user input)
- `userOutput` (for screen output)
- `file` (for file access)

**Example:**
- `semWait userOutput` - Acquire screen output resource.
- `semSignal userOutput` - Release screen output resource.

**Rules:**
- Only one process can use a resource at a time.
- If a resource is busy, requesting processes are blocked and added to the respective blocked queues.
- Unblocking is based on priority: highest priority process waiting for a resource is unblocked first.
- Unblocked processes return to the ready queue at their priority level.

---

## GUI Requirements for the Scheduler Simulation

The GUI must display real-time information on processes, memory, and resources.

### Main Features

1. **Main Dashboard**
   - **Overview:** Total processes, current clock cycle, active scheduling algorithm.
   - **Process List:** Shows all processes with ID, state (Ready/Running/Blocked), priority, memory boundaries, program counter.
   - **Queue Section:** Ready queue, blocking queue, running process details (including current instruction and time in queue).

2. **Scheduler Control Panel**
   - **Algorithm Selection:** Dropdown for FCFS, RR (with adjustable quantum), and MLFQ.
   - **Controls:** Start, Stop, Reset simulation.
   - **Quantum Adjustment:** Input for setting RR quantum.

3. **Resource Management Panel**
   - **Mutex Status:** Shows which process is holding or waiting for userInput, userOutput, or file resources.
   - **Blocked Queue:** Lists processes waiting for resources and their priorities.

4. **Memory Viewer**
   - Visualizes allocation of all 60 memory words.

5. **Log & Console Panel**
   - **Execution Log:** Real-time log of executed instructions (shows current instruction, process ID, system reaction).
   - **Event Messages:** Real-time system status (e.g., when processes are blocked/unblocked).

6. **Process Creation and Configuration**
   - **Add Process:** Load a new process file via file prompt.
   - **Process Configuration:** Set arrival time for each process before starting simulation.

7. **Visualized Execution**
   - **Step-by-Step Execution:** Advance simulation one clock cycle at a time.
   - **Auto Execution:** Run continuously until completion or stopped.

---

### Additional Notes

- The process scheduling order can be changed by the user.
- Memory must be displayed in a human-readable format every clock cycle.
- GUI updates after each clock cycle to reflect the latest data (current process, resource usage, memory allocation).
