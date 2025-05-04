#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdbool.h> // Added for boolean type
#include <time.h>
#include <math.h>
#include <ctype.h>  // Added for isdigit function

#define MAX_PROCESSES 10
#define RR_QUANTUM 4  // Default quantum for Round Robin
#define MAX_PRIORITY 3  
#define MAX_VARIABLES 100
#define MAX_LINE_LENGTH 256
#define MAX_FILENAME_LENGTH 50
#define MAX_CONTENT_LENGTH 1024
#define MAX_INSTRUCTIONS 20
#define INSTRUCTION_SIZE 50
#define STATE_SIZE 20
#define NUM_MLFQ_LEVELS 4  // 4 levels for MLFQ


//================================  Data Structures ===========================

typedef struct PCB {
    int processID;
    char processState[50];
    int currentPriority; 
    int programCounter;
    int lowerMemoryBound;                         
    int upperMemoryBound;
    struct PCB* next; // points to the next process in the queue
} PCB;

typedef struct {
    int processID;
    char* name;
    char* value; 
} memoryWord;

typedef enum {// Algorithms(made as enum to be given by the user)
    FCFS,
    ROUND_ROBIN,
    MLFQ
} SchedulingAlgorithm;

typedef struct {
    char name[50];
    char value[1000];
    int is_numeric; // 1 if number, 0 if string
} Variable;

// Queue node for scheduling
typedef struct QueueNode {
    PCB* pcb;
    struct QueueNode* next;
} QueueNode;

// Queue structure
typedef struct {
    QueueNode* front;
    QueueNode* rear;
} Queue;

// Process table entry
typedef struct {
    PCB* pcb;
    int arrivalTime;
    int burstTime;
    int executedTime;
    bool hasArrived;
    bool isComplete;
    int currentQueueLevel;  // For MLFQ: 0-3 (0 is highest priority)
    int quantumRemaining;   // For MLFQ: track quantum within current level
} ProcessTableEntry;

// Multi-Level Feedback Queue
typedef struct {
    Queue queues[NUM_MLFQ_LEVELS]; // 4 queue levels
    int timeQuantums[NUM_MLFQ_LEVELS]; // Time quantum for each level
} MLFQScheduler;

typedef struct Mutex {
    char resource[20];
    int locked; // is it available or not (0=available, 1= locked)
    PCB* owner; //points to the process that is using the resource to make sure it unlocks it later (and not another process)
    PCB* blockedQueue; // queue that contains the blocked processes
} Mutex;

// Global variables
memoryWord memory[60];
int memorySize = 60;
ProcessTableEntry* processTable;
int numProcesses;
Queue readyQueue;
MLFQScheduler mlfqScheduler;
SchedulingAlgorithm algorithm;
int currentTime;
int processCount = -1;
Variable variables[MAX_VARIABLES];
int var_count = 0;
Mutex fileMutex, inputMutex, outputMutex;


// Function prototypes
void initialize_queue(Queue* q);
bool is_queue_empty(Queue* q);
void enqueue(Queue* q, PCB* pcb);
PCB* dequeue(Queue* q);
void display_queue(Queue* q);
void initialMemory();
void initMutex(Mutex* mutex, const char* name);
void semWait(Mutex* mutex, PCB* process);
void semSignal(Mutex* mutex, PCB* process);
void execute(char* line, PCB* currentProcess);

//================================ Queue Operations ===========================

// Initialize queue
void initialize_queue(Queue* q) {
    q->front = NULL;
    q->rear = NULL;
}

// Check if queue is empty
bool is_queue_empty(Queue* q) {
    return (q->front == NULL);
}

// Add process to queue
void enqueue(Queue* q, PCB* pcb) {
    QueueNode* newNode = (QueueNode*)malloc(sizeof(QueueNode));
    if (!newNode) {
        printf("Error: Failed to allocate memory for queue node\n");
        return;
    }
    
    newNode->pcb = pcb;
    newNode->next = NULL;
    
    if (is_queue_empty(q)) {
        q->front = newNode;
        q->rear = newNode;
    } else {
        q->rear->next = newNode;
        q->rear = newNode;
    }
}

// Remove and return process from queue
PCB* dequeue(Queue* q) {
    if (is_queue_empty(q)) {
        return NULL;
    }
    
    QueueNode* temp = q->front;
    PCB* pcb = temp->pcb;
    
    q->front = q->front->next;
    if (q->front == NULL) {
        q->rear = NULL;
    }
    
    free(temp);
    return pcb;
}

//================================ Memory Management ===========================

// Memory management functions
int readProgramFile(const char* fileName, char*** instructions) {
    FILE* file = fopen(fileName, "r");
    if (!file) {
        printf("Failed to open file: %s\n", fileName);
        return -1;
    }

    // Count lines first
    int IC = 0;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), file)) {
        if (buffer[0] != '\n' && buffer[0] != '#') IC++;
    }
    
    rewind(file);
    
    // Allocate instruction array
    *instructions = (char**)malloc(IC * sizeof(char*));
    if (!*instructions) {
        printf("Memory allocation failed\n");
        fclose(file);
        return -1;
    }
    
    // Read instructions
    int index = 0;
    while (fgets(buffer, sizeof(buffer), file)) {
        if (buffer[0] == '\n' || buffer[0] == '#') continue;
        
        buffer[strcspn(buffer, "\n")] = 0; // Remove newline
        (*instructions)[index] = strdup(buffer);
        index++;
    }
    
    fclose(file);
    return IC;
}

void initialMemory() {
    for (int i = 0; i < 60; i++) {
        memory[i].name = NULL;
        memory[i].value = NULL;
        memory[i].processID = -1;
    }
}

int memoryAllocate(int processID, int IC) {  // IC instruction count
    int firstFree = -1;
    bool flag = false;
    int placeNeeded = IC + 3 + 6;  // Instructions + var + PCB entries
    for (int i = 0; i < 60; i++) {
        if (memory[i].name == NULL) {
            for(int j=i; j<placeNeeded+i && j<60; j++){
                if(memory[j].name!=NULL){
                    i=j;
                    flag=false;
                    break;
                }
                else 
                    flag = true;
            }    
        } if (flag == true) {
            firstFree = i;
            break;
        } 
    } 
    return firstFree;  // Return the first free memory location found
}

void memorydeallocate(int processID) {
    for (int i = 0; i < 60; i++) {
        if (memory[i].processID == processID) {
            if (memory[i].name != NULL) free(memory[i].name);
            if (memory[i].value != NULL) free(memory[i].value);
            memory[i].processID = -1;  
            memory[i].name = NULL;
            memory[i].value = NULL;
            
            printf("Freed memory word %d\n", i);
        }
    }
}

PCB* createProcess(const char* fileName) {
    char** instructions;
    int IC = readProgramFile(fileName, &instructions);
    
    if (IC <= 0) {
        printf("Failed to read program file or no instructions found\n");
        return NULL;
    }
    
    processCount++;
    int p = memoryAllocate(processCount, IC); // p is the first free memory location
    
    if (p == -1) {
        printf("Not enough memory to allocate process\n");
        // Free instructions
        for (int i = 0; i < IC; i++) {
            free(instructions[i]);
        }
        free(instructions);
        return NULL;
    }

    PCB* newProcess = (PCB*)malloc(sizeof(PCB));
    if (!newProcess) {
        printf("Failed to allocate PCB\n");
        // Free instructions
        for (int i = 0; i < IC; i++) {
            free(instructions[i]);
        }
        free(instructions);
        return NULL;
    }

    // Initialize PCB fields
    newProcess->processID = processCount;
    strcpy(newProcess->processState, "READY");
    newProcess->currentPriority = 0; // Start with highest priority
    newProcess->programCounter = p;
    newProcess->lowerMemoryBound = p;
    newProcess->upperMemoryBound = p + IC + 6 + 3-1;
    newProcess->next = NULL;
    
    // Store instructions to memory
    int j = 0;
    for (int i = p; i < p + IC; i++) {
        char temp[20];
        sprintf(temp, "Instruction %d", j);
        memory[i].name = strdup(temp);
        memory[i].value = strdup(instructions[j]);
        memory[i].processID = processCount;
        j++;
    }
    
    // Free instructions array as they've been copied to memory
    for (int i = 0; i < IC; i++) {
        free(instructions[i]);
    }
    free(instructions);
    
    // Add variables to memory 
    for(int i=p+IC; i<p+IC+3; i++){
        memory[i].name = strdup("Variable");
        memory[i].value = strdup("NULL");
        memory[i].processID = processCount; 
    }

    // Skip 3 memory locations (the gap)
    int currentIndex = p + IC + 3;  // Starting index for PCB
    
    // Add PCB info to memory - all 6 fields in order
    char temp[50];
    
    // 1. Process ID
    sprintf(temp, "%d", newProcess->processID);
    memory[currentIndex].name = strdup("PCB_ID");
    memory[currentIndex].value = strdup(temp);
    memory[currentIndex].processID = newProcess->processID;
    currentIndex++;
    
    // 2. Process State
    memory[currentIndex].name = strdup("processState");
    memory[currentIndex].value = strdup(newProcess->processState);
    memory[currentIndex].processID = newProcess->processID;
    currentIndex++;
    
    // 3. Current Priority
    sprintf(temp, "%d", newProcess->currentPriority);
    memory[currentIndex].name = strdup("currentPriority");
    memory[currentIndex].value = strdup(temp);
    memory[currentIndex].processID = newProcess->processID;
    currentIndex++;
    
    // 4. Program Counter
    sprintf(temp, "%d", newProcess->programCounter);
    memory[currentIndex].name = strdup("programCounter");
    memory[currentIndex].value = strdup(temp);
    memory[currentIndex].processID = newProcess->processID;
    currentIndex++;
    
    // 5. Lower Memory Bound
    sprintf(temp, "%d", newProcess->lowerMemoryBound);
    memory[currentIndex].name = strdup("lowerMemoryBound");
    memory[currentIndex].value = strdup(temp);
    memory[currentIndex].processID = newProcess->processID;
    currentIndex++;
    
    // 6. Upper Memory Bound
    sprintf(temp, "%d", newProcess->upperMemoryBound);
    memory[currentIndex].name = strdup("upperMemoryBound");
    memory[currentIndex].value = strdup(temp);
    memory[currentIndex].processID = newProcess->processID;
    
    printf("Created process with ID %d from file %s\n", processCount, fileName);
    printf("PCB stored at memory locations %d to %d\n", p + IC + 3, p + IC + 3 + 5);
    return newProcess;
}

void displayMemory() {
    printf("\n============= MEMORY CONTENTS =============\n");
    for (int i = 0; i < 60; i++) {
        if (memory[i].processID != -1) {
            printf("[%2d] PID: %2d | Name: %-20s | Value: %s\n", 
                  i, memory[i].processID, 
                  memory[i].name ? memory[i].name : "NULL", 
                  memory[i].value ? memory[i].value : "NULL");
        } else {
            // Optionally show empty slots to better visualize the gaps
            printf("[%2d] EMPTY\n", i);
        }
    }
    printf("===========================================\n\n");
}

//================================ ISA Interpreter ===========================

// Find variable by name
Variable* findVariable(const char* name) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(variables[i].name, name) == 0) {
            return &variables[i];
        }
    }
    return NULL;
}

// Check if a string is numeric
int isNumeric(const char* value) {
    int start = (value[0] == '-') ? 1 : 0;  // Handle negative numbers
    for (int i = start; value[i] != '\0'; i++) {
        if (!isdigit(value[i])) {
            return 0;  // Not a numeric string
        }
    }
    return 1;  // Is numeric
}

// Set variable
void setVariable(const char* name, const char* value) {
    Variable* var = findVariable(name);
    if (var == NULL) {
        if (var_count < MAX_VARIABLES) {
            var = &variables[var_count++];
            strcpy(var->name, name);
        } else {
            printf("Error: Maximum number of variables reached\n");
            return;
        }
    }
    strcpy(var->value, value);
    var->is_numeric = isNumeric(value);  // Set the numeric flag based on the value
}

// print
void print(const char* arg) {
    Variable* var = findVariable(arg);
    if (var != NULL) {
        printf("%s\n", var->value);
    } else {
        printf("Error: Variable '%s' not found\n", arg);
    }
}

// assign
// Modified assign function to properly handle user input
void assign(const char* varName, const char* value, PCB* currentProcess) {
    if (strcmp(value, "input") == 0) {
        char input[100];
        int valid = 0;
        
        // If we need a numeric value, keep asking until we get one
        if (varName[0] >= 'a' && varName[0] <= 'z') {
            while (!valid) {
                printf("Please enter an integer value for %s: ", varName);
                scanf("%s", input);
                input[strcspn(input, "\n")] = '\0';  // Remove newline
                
                // Check if input is a valid integer
                valid = 1;
                for (int i = 0; input[i] != '\0'; i++) {
                    if (i == 0 && input[i] == '-') continue;  // Allow negative numbers
                    if (!isdigit(input[i])) {
                        valid = 0;
                        printf("Error: Please enter a valid integer.\n");
                        break;
                    }
                }
            }
        } else {
            printf("Please enter a value for %s: ", varName);
            fgets(input, sizeof(input), stdin);
            input[strcspn(input, "\n")] = '\0';  // Remove newline
        }
        
        setVariable(varName, input);
    }
    else if (strncmp(value, "readFile", 8) == 0) {
        char filename[100];
        sscanf(value, "readFile %s", filename);

        Variable* fileVar = findVariable(filename);
        const char* fname = fileVar ? fileVar->value : filename;

        FILE* f = fopen(fname, "r");
        if (!f) {
            printf("Error: Cannot read file '%s'\n", fname);
            return;
        }

        char buffer[MAX_CONTENT_LENGTH] = "";
        fread(buffer, 1, MAX_CONTENT_LENGTH, f);
        fclose(f);
        buffer[strcspn(buffer, "\0")] = '\0';
        setVariable(varName, buffer);  // Store content as string
    }
    else {
        // Check if value is a number
        if (isNumeric(value)) {
            setVariable(varName, value);  // Set numeric value
        } else {
            // Check if value is another variable
            Variable* srcVar = findVariable(value);
            if (srcVar != NULL) {
                setVariable(varName, srcVar->value);  // Set value from another variable
            } else {
                printf("Error: Value '%s' is not valid\n", value);
            }
        }
    }
    
    // Update memory
    if (currentProcess != NULL) {
        int found = 0;
        int start = currentProcess->lowerMemoryBound;
        int end = currentProcess->upperMemoryBound - 6;  // Skip PCB area
        
        // Find variable slot in memory
        for (int i = start + (end - start - 3); i < end; i++) {
            // Look for an empty variable slot or existing variable with same name
            if ((strcmp(memory[i].name, "Variable") == 0 && strcmp(memory[i].value, "NULL") == 0) ||
                (memory[i].name != NULL && strcmp(memory[i].name, varName) == 0)) {
                
                // Update variable in memory
                if (memory[i].name != NULL && strcmp(memory[i].name, "Variable") == 0) {
                    free(memory[i].name);
                    memory[i].name = strdup(varName);
                }
                
                if (memory[i].value != NULL) {
                    free(memory[i].value);
                }
                
                // Get the actual value to store
                const char* valueToStore = value;
                if (strcmp(value, "input") == 0) {
                    Variable* var = findVariable(varName);
                    if (var != NULL) {
                        valueToStore = var->value;
                    }
                } else if (strncmp(value, "readFile", 8) == 0) {
                    Variable* var = findVariable(varName);
                    if (var != NULL) {
                        valueToStore = var->value;
                    }
                }
                
                memory[i].value = strdup(valueToStore);
                found = 1;
                break;
            }
        }
        
        if (!found) {
            printf("Error: No space available in process memory for variable %s\n", varName);
        }
    }
}

// printFromTo -> Program 1
void printFromTo(const char* startStr, const char* endStr) {
    int start, end;
    Variable* startVar = findVariable(startStr);
    Variable* endVar = findVariable(endStr);

    if (startVar != NULL && startVar->is_numeric) {
        start = atoi(startVar->value);
    } else if (isNumeric(startStr)) {
        start = atoi(startStr);
    } else {
        printf("Error: Invalid start value '%s'\n", startStr);
        return;
    }

    if (endVar != NULL && endVar->is_numeric) {
        end = atoi(endVar->value);
    } else if (isNumeric(endStr)) {
        end = atoi(endStr);
    } else {
        printf("Error: Invalid end value '%s'\n", endStr);
        return;
    }

    int step = (start <= end) ? 1 : -1;
    for (int i = start; (step == 1 ? i <= end : i >= end); i += step) {
        printf("%d ", i);
    }
    printf("\n");
}

// writeFile -> Program 2
void writeFile(const char* filename, const char* content) {
    Variable* fileVar = findVariable(filename);
    const char* fileNameStr = fileVar ? fileVar->value : filename;

    const char* actualContent = content;
    Variable* contentVar = findVariable(content);
    if (contentVar != NULL) {
        actualContent = contentVar->value;
    } else if (content[0] == '"') {
        actualContent = content + 1;
    } else {
        printf("Error: Content must be a string or variable\n");
        return;
    }

    FILE* file = fopen(fileNameStr, "w");
    if (file == NULL) {
        printf("Error: Could not open file '%s' for writing\n", fileNameStr);
        return;
    }

    fprintf(file, "%s", actualContent);
    fclose(file);
    printf("Data written to '%s'\n", fileNameStr);
}

// readFile -> Program 3
void readFile(const char* filename) {
    Variable* fileVar = findVariable(filename);
    const char* fileNameStr = fileVar ? fileVar->value : filename;

    FILE* file = fopen(fileNameStr, "r");
    if (file == NULL) {
        printf("Error: Could not open file '%s' for reading\n", fileNameStr);
        return;
    }

    char buffer[MAX_CONTENT_LENGTH];
    printf("Contents of '%s':\n", fileNameStr);
    while (fgets(buffer, sizeof(buffer), file) != NULL) {
        printf("%s", buffer);
    }
    fclose(file);
}

// Add process to priority-sorted blocked queue
void addToBlockedQueue(PCB** head, PCB* newProcess) {
    if (*head == NULL || (*head)->currentPriority < newProcess->currentPriority) { 
        newProcess->next = *head; // puts process at the front if the queue is empty or the new process has the highest priority
        *head = newProcess;
    } else {
        PCB* current = *head;
        while (current->next != NULL && current->next->currentPriority >= newProcess->currentPriority) {
            current = current->next; //loop to know where to put the new process based on its priority
        }
        newProcess->next = current->next;
        current->next = newProcess;
    }
}

// Remove and return highest priority process
PCB* popBlockedQueue(PCB** head) {
    if (*head == NULL) 
        return NULL; // empty queue
    PCB* top = *head;
    *head = (*head)->next;
    top->next = NULL;
    return top;
}

// Helper to initialize a mutex
void initMutex(Mutex* mutex, const char* name) {
    strcpy(mutex->resource, name);
    mutex->locked = 0;                //the resource is initialized as available
    mutex->owner = NULL;
    mutex->blockedQueue = NULL;
}

void semWait(Mutex* mutex, PCB* process) {
    if (!mutex->locked) {
        mutex->locked = 1;   // if resource is available lock it and change owner
        mutex->owner = process;
        printf("Process %d acquired %s\n", process->processID, mutex->resource);
    } else {
        printf("Process %d is BLOCKED on %s\n", process->processID, mutex->resource);
        strcpy(process->processState, "BLOCKED");   //if it is not available block process
        addToBlockedQueue(&mutex->blockedQueue, process);
    }
}

void semSignal(Mutex* mutex, PCB* process) {
    if (mutex->owner != process) {  //if the process is not the owner ignore the release
        printf("Process %d cannot release mutex it doesn't own.\n", process->processID);
        return; // Exit early — no changes allowed
    }
    if (mutex->blockedQueue != NULL) {
        PCB* nextProcess = popBlockedQueue(&mutex->blockedQueue);
        strcpy(nextProcess->processState, "READY");  // if there is a process waiting for the resource let it lock it
        mutex->owner = nextProcess;
        enqueue(&readyQueue ,nextProcess);
        printf("Process %d is UNBLOCKED and acquired %s\n", nextProcess->processID, mutex->resource);
    } else {
        mutex->locked = 0;
        mutex->owner = NULL;    // else just release the resource
        printf("%s is now FREE\n", mutex->resource);
    }
}

// Execute instruction
void execute(char* line, PCB* currentProcess) {
    char command[20], arg1[100], arg2[100];
    int numArgs = sscanf(line, "%19s %99s %99[^\n]", command, arg1, arg2);

    if (strcmp(command, "print") == 0 && numArgs >= 2) {
        print(arg1);
    } 
    else if (strcmp(command, "assign") == 0 && numArgs == 3) {
        assign(arg1, arg2, currentProcess);
    }
    else if (strcmp(command, "printFromTo") == 0 && numArgs == 3) {
        printFromTo(arg1, arg2);
    }
    else if (strcmp(command, "writeFile") == 0 && numArgs == 3) {
        writeFile(arg1, arg2);
    }
    else if (strcmp(command, "readFile") == 0 && numArgs == 2) {
        readFile(arg1);
    }
    else if (strcmp(command, "semWait") == 0 && numArgs == 2) {
        if (strcmp(arg1, "file") == 0)
            semWait(&fileMutex, currentProcess);
        else if (strcmp(arg1, "userInput") == 0)
            semWait(&inputMutex, currentProcess);
        else if (strcmp(arg1, "userOutput") == 0)
            semWait(&outputMutex, currentProcess);
    }
    else if (strcmp(command, "semSignal") == 0 && numArgs == 2) {
        if (strcmp(arg1, "file") == 0)
        semSignal(&fileMutex, currentProcess);
    else if (strcmp(arg1, "userInput") == 0)
    semSignal(&inputMutex, currentProcess);
    else if (strcmp(arg1, "userOutput") == 0)
    semSignal(&outputMutex, currentProcess);
    }
    else {
        printf("Error: Unknown command or incorrect arguments for '%s'\n", command);
    }
}

//================================ Scheduling Algorithms ===========================

// Display the contents of a queue
void display_queue(Queue* q) {
    if (is_queue_empty(q)) {
        printf("Queue is empty\n");
        return;
    }
    
    QueueNode* current = q->front;
    printf("Queue contents: ");
    while (current != NULL) {
        printf("P%d ", current->pcb->processID);
        current = current->next;
    }
    printf("\n");
}

// Display all queues in MLFQ
void display_all_queues(MLFQScheduler* s) {
    for (int i = 0; i < NUM_MLFQ_LEVELS; i++) {
        printf("Level %d (Quantum: %d): ", i + 1, s->timeQuantums[i]);
        display_queue(&s->queues[i]);
    }
    printf("\n");
}

// Initialize the MLFQ scheduler
void initialize_mlfq_scheduler(MLFQScheduler* s) {
    // Set time quantums for each level
    s->timeQuantums[0] = 1;  // Level 1: 1 time unit
    s->timeQuantums[1] = 2;  // Level 2: 2 time units
    s->timeQuantums[2] = 4;  // Level 3: 4 time units
    s->timeQuantums[3] = 8;  // Level 4: RR with 8 time units
    
    // Initialize queues
    for (int i = 0; i < NUM_MLFQ_LEVELS; i++) {
        initialize_queue(&s->queues[i]);
    }
    
    printf("MLFQ scheduler initialized with %d levels:\n", NUM_MLFQ_LEVELS);
    for (int i = 0; i < NUM_MLFQ_LEVELS; i++) {
        printf("Level %d: Time Quantum = %d\n", i + 1, s->timeQuantums[i]);
    }
}

// Check for process arrivals at the current time
void check_for_process_arrivals(int currentTime) {
    for (int i = 0; i < numProcesses; i++) {
        if (processTable[i].arrivalTime <= currentTime && !processTable[i].hasArrived && !processTable[i].isComplete) {
            printf("Time %d: Process %d arrived\n", currentTime, processTable[i].pcb->processID);
            strcpy(processTable[i].pcb->processState, "READY");
            
            if (algorithm == MLFQ) {
                // For MLFQ, add to the highest priority queue (level 0)
                enqueue(&mlfqScheduler.queues[0], processTable[i].pcb);
                processTable[i].currentQueueLevel = 0;
                processTable[i].quantumRemaining = mlfqScheduler.timeQuantums[0];
                printf("Process %d added to highest priority queue (Level 1, Queue 0)\n", 
                       processTable[i].pcb->processID);
            } else {
                // For FCFS and RR, add to the main ready queue
                enqueue(&readyQueue, processTable[i].pcb);
            }
            
            processTable[i].hasArrived = true;
            
            // Display queues after process arrival
            if (algorithm == MLFQ) {
                printf("MLFQ queues after process arrival:\n");
                display_all_queues(&mlfqScheduler);
            } else {
                printf("Ready queue after process arrival:\n");
                display_queue(&readyQueue);
            }
        }
    }
}

// Check if all processes have completed execution
bool all_processes_complete() {
    for (int i = 0; i < numProcesses; i++) {
        if (!processTable[i].isComplete) {
            return false;
        }
    }
    return true;
}

// FCFS scheduling algorithm
void run_fcfs_scheduler() {
    printf("Starting FCFS scheduler\n");
    
    currentTime = 0;
    
    while (!all_processes_complete()) {
        // Check for new arrivals
        check_for_process_arrivals(currentTime);
        
        // Get next process from ready queue
        PCB* currentProcess = dequeue(&readyQueue);
        
        if (currentProcess != NULL) {
            strcpy(currentProcess->processState, "RUNNING");
            printf("Time %d: Process %d is now running\n", currentTime, currentProcess->processID);
            
            // Find the process entry to determine burst time
            int burstTime = 0;
            int processIndex = -1;
            for (int i = 0; i < numProcesses; i++) {
                if (processTable[i].pcb->processID == currentProcess->processID) {
                    burstTime = processTable[i].burstTime;
                    processIndex = i;
                    break;
                }
            }
            
            // Execute all instructions of this process
            int remainingInstructions = burstTime;
            while (remainingInstructions > 0 && 
                  (currentProcess->programCounter) < currentProcess->upperMemoryBound) {
                
                int memoryIndex = currentProcess->programCounter;
                
                // Execute the instruction
                printf("Time %d: Executing process %d, instruction: %s\n", 
                       currentTime, currentProcess->processID, memory[memoryIndex].value);
                
                execute(memory[memoryIndex].value, currentProcess); // Execute the instruction      
                // Update program counter
                currentProcess->programCounter++;
                
                // Advance time
                currentTime++;
                
                // Decrement remaining instructions
                remainingInstructions--;
                
                // Check for new arrivals after each time unit
                check_for_process_arrivals(currentTime);
            }
            
            // Process is now complete
            printf("Process %d has completed execution at time %d\n", currentProcess->processID, currentTime);
            strcpy(currentProcess->processState, "TERMINATED");
            
            // Mark process as complete in process table
            if (processIndex != -1) {
                processTable[processIndex].isComplete = true;
            }
        } else {
            printf("Time %d: CPU idle - no process in ready queue\n", currentTime);
            // Advance time if no process is available
            currentTime++;
            sleep(1); // Simulate idle time
        }
    }
    
    printf("All processes have completed execution. Total time: %d\n", currentTime);
}

// Round Robin scheduling algorithm
void run_rr_scheduler(int timeQuantum) {
    int currentTime = 0;
    int completedProcesses = 0;
    PCB* currentProcess = NULL;
    int remainingQuantum = 0;
    
    printf("\nStarting Round Robin Scheduler with Time Quantum = %d\n", timeQuantum);
    
    while (completedProcesses < numProcesses) {
        // Check for new arrivals
        check_for_process_arrivals(currentTime);
        
        // If no process is currently running, get one from the ready queue
        if (currentProcess == NULL) {
            currentProcess = dequeue(&readyQueue);
            if (currentProcess != NULL) {
                strcpy(currentProcess->processState, "RUNNING");
                printf("Time %d: Process %d is now running\n", currentTime, currentProcess->processID);
                remainingQuantum = timeQuantum;
            }
        }
        
        if (currentProcess != NULL) {
            int processIndex = -1;
            for (int i = 0; i < numProcesses; i++) {
                if (processTable[i].pcb->processID == currentProcess->processID) {
                    processIndex = i;
                    break;
                }
            }
            
            // Execute one instruction
            if (currentProcess->programCounter < currentProcess->upperMemoryBound) {
                int memoryIndex = currentProcess->programCounter;
                
                printf("Time %d: Executing process %d, instruction: %s (Quantum remaining: %d)\n", 
                       currentTime, currentProcess->processID, memory[memoryIndex].value, remainingQuantum);
                
                execute(memory[memoryIndex].value, currentProcess);                
                currentProcess->programCounter++;
                processTable[processIndex].executedTime++;
                
                // Decrement remaining quantum
                remainingQuantum--;
                //check if process was blocked
                if (strcmp(currentProcess->processState, "BLOCKED") == 0) {
                    printf("Time %d: Process %d was blocked, moving out of CPU\n",
                           currentTime + 1, currentProcess->processID);
                    currentProcess = NULL;  
                }
                // Check if process has completed all its instructions
                else if (processTable[processIndex].executedTime >= processTable[processIndex].burstTime ||
                    currentProcess->programCounter >= currentProcess->upperMemoryBound) {
                    printf("Process %d has completed execution at time %d\n", currentProcess->processID, currentTime + 1);
                    strcpy(currentProcess->processState, "TERMINATED");
                    processTable[processIndex].isComplete = true;
                    completedProcesses++;
                    currentProcess = NULL;
                } 
                // Check if time quantum has expired
                else if (remainingQuantum <= 0) {
                    printf("Time %d: Process %d time quantum expired, moving to ready queue\n", 
                           currentTime + 1, currentProcess->processID);
                    strcpy(currentProcess->processState, "READY");
                    enqueue(&readyQueue, currentProcess);
                    currentProcess = NULL;
                }
            }
        } else {
            printf("Time %d: CPU idle - no process in ready queue\n", currentTime);
        }
        
        // Advance time
        currentTime++;
        
        // Add some delay to observe the scheduling
        usleep(200000); // 200ms delay
    }
    
    printf("All processes have completed execution. Total time: %d\n", currentTime);
}

// MLFQ scheduling algorithm
void run_mlfq_scheduler() {
    printf("\nStarting Multi-Level Feedback Queue Scheduler\n");
    
    int currentTime = 0;
    int completedProcesses = 0;
    PCB* currentProcess = NULL;
    int currentLevel = 0;
    int remainingQuantum = 0;
    
    while (completedProcesses < numProcesses) {
        // Check for new arrivals
        check_for_process_arrivals(currentTime);
        
        // If no process is currently running, get one from the highest priority queue
       if (currentProcess == NULL) {
    for (int level = 0; level < NUM_MLFQ_LEVELS; level++) {
        int queueSize = mlfqScheduler.queues[level].rear - mlfqScheduler.queues[level].front;
        for (int i = 0; i < queueSize; i++) {
            PCB* candidate = dequeue(&mlfqScheduler.queues[level]);
            if (strcmp(candidate->processState, "READY") == 0) {
                currentProcess = candidate;
                currentLevel = level;
                remainingQuantum = mlfqScheduler.timeQuantums[level];
                strcpy(currentProcess->processState, "RUNNING");
                printf("Time %d: Process %d from Level %d is now running (Quantum: %d)\n", 
                       currentTime, currentProcess->processID, level + 1, remainingQuantum);
                break; // <- break inner loop
            } else {
                enqueue(&mlfqScheduler.queues[level], candidate); 
            }
        }
        if (currentProcess != NULL) break; // <- break outer loop
    }
}

        
        if (currentProcess != NULL) {
            int processIndex = -1;
            for (int i = 0; i < numProcesses; i++) {
                if (processTable[i].pcb->processID == currentProcess->processID) {
                    processIndex = i;
                    break;
                }
            }
            
            // Execute one instruction
            if (currentProcess->programCounter < currentProcess->upperMemoryBound) {
                int memoryIndex = currentProcess->programCounter;
                //check if process was blocked
             //   if (strcmp(currentProcess->processState, "BLOCKED") == 0) {
              //      printf("Time %d: Process %d got blocked, pausing execution\n", currentTime + 1, currentProcess->processID);
              //      currentProcess = NULL;
              //  }
             //  else{
                 printf("Time %d: Executing process %d, instruction: %s (Level: %d, Quantum remaining: %d)\n", 
                       currentTime, currentProcess->processID, memory[memoryIndex].value, 
                       currentLevel + 1, remainingQuantum);
                
                execute(memory[memoryIndex].value, currentProcess);                
                currentProcess->programCounter++;
                processTable[processIndex].executedTime++;
                
                // Decrement remaining quantum
                remainingQuantum--;
                // Check if process has completed all its instructions
                if (processTable[processIndex].executedTime >= processTable[processIndex].burstTime ||
                    currentProcess->programCounter >= currentProcess->upperMemoryBound) {
                    printf("Process %d has completed execution at time %d\n", currentProcess->processID, currentTime + 1);
                    strcpy(currentProcess->processState, "TERMINATED");
                    processTable[processIndex].isComplete = true;
                    completedProcesses++;
                    currentProcess = NULL;
                }
                // Check if time quantum has expired
                else if (remainingQuantum <= 0) {
                    printf("Time %d: Process %d time quantum expired\n", currentTime + 1, currentProcess->processID);
                    
                    // Move to lower priority queue if not already at lowest
                    int newLevel = (currentLevel < NUM_MLFQ_LEVELS - 1) ? currentLevel + 1 : currentLevel;
                    if (strcmp(currentProcess->processState, "BLOCKED") != 0) {
                    strcpy(currentProcess->processState, "READY");}
                    processTable[processIndex].currentQueueLevel = newLevel;
                    processTable[processIndex].quantumRemaining = mlfqScheduler.timeQuantums[newLevel];
                    
                    printf("Moving Process %d to Level %d\n", currentProcess->processID, newLevel + 1);
                    enqueue(&mlfqScheduler.queues[newLevel], currentProcess);
                    currentProcess = NULL;
                }
            }
        } else {
            printf("Time %d: CPU idle - no process in any queue\n", currentTime);
        }
        
        // Advance time
        currentTime++;
        
        // Add some delay to observe the scheduling
        usleep(200000); // 200ms delay
    }
    
    printf("All processes have completed execution. Total time: %d\n", currentTime);
}

// Display PCB information
void displayPCB(PCB* pcb) {
    if (pcb == NULL) {
        printf("PCB is NULL\n");
        return;
    }
    
    printf("\n===== PCB Information =====\n");
    printf("Process ID: %d\n", pcb->processID);
    printf("Process State: %s\n", pcb->processState);
    printf("Current Priority: %d\n", pcb->currentPriority);
    printf("Program Counter: %d\n", pcb->programCounter);
    printf("Memory Range: %d - %d\n", pcb->lowerMemoryBound, pcb->upperMemoryBound);
    printf("===========================\n");
}

// Display process table information
void displayProcessTable() {
    printf("\n============ Process Table ============\n");
    printf("PID\tArrival\tBurst\tExecuted\tState\tQueue Level\n");
    for (int i = 0; i < numProcesses; i++) {
        printf("%d\t%d\t%d\t%d\t\t%s\t%d\n", 
                processTable[i].pcb->processID,
                processTable[i].arrivalTime,
                processTable[i].burstTime,
                processTable[i].executedTime,
                processTable[i].pcb->processState,
                processTable[i].currentQueueLevel);
    }
    printf("======================================\n");
}

// Clean up resources
void cleanupResources() {
    // Free memory allocated for processes
    for (int i = 0; i < numProcesses; i++) {
        free(processTable[i].pcb);
    }
    free(processTable);
    
    // Free memory words
    for (int i = 0; i < memorySize; i++) {
        if (memory[i].name != NULL) {
            free(memory[i].name);
        }
        if (memory[i].value != NULL) {
            free(memory[i].value);
        }
    }
    
    printf("Resources cleaned up\n");
}

int main(int argc, char *argv[]) {
    // Initialize memory
    initialMemory();

    //initialize mutex
    initMutex(&fileMutex, "file");
    initMutex(&inputMutex, "userInput");
    initMutex(&outputMutex, "userOutput");

    
    // Initialize MLFQ scheduler
    initialize_mlfq_scheduler(&mlfqScheduler);
    
    // Initialize ready queue
    initialize_queue(&readyQueue);
    
    // Set scheduling algorithm (can be changed via command line)
        algorithm = MLFQ;
        //algorithm = FCFS;  // Default
    
    // Parse command-line arguments
    if (argc > 1) {
        if (strcmp(argv[1], "rr") == 0) {
            algorithm = ROUND_ROBIN;
            printf("Using Round Robin scheduling\n");
        } else if (strcmp(argv[1], "mlfq") == 0) {
            algorithm = MLFQ;
            printf("Using Multi-Level Feedback Queue scheduling\n");
        } else {
            printf("Using First-Come First-Served scheduling\n");
        }
    }
    
    // Read input file or hardcode processes for testing
    printf("Enter number of processes: ");
    scanf("%d", &numProcesses);
    
    // Allocate process table
    processTable = (ProcessTableEntry*)malloc(numProcesses * sizeof(ProcessTableEntry));
    if (!processTable) {
        printf("Failed to allocate process table\n");
        return 1;
    }
    
    // Create processes and initialize process table
    for (int i = 0; i < numProcesses; i++) {
        char filename[MAX_FILENAME_LENGTH];
        printf("Enter program file for process %d: ", i + 1);
        scanf("%s", filename);
        
        PCB* pcb = createProcess(filename);
        if (pcb == NULL) {
            printf("Failed to create process %d\n", i + 1);
            i--; // Try again
            continue;
        }
        
        processTable[i].pcb = pcb;
        printf("Enter arrival time for process %d: ", i + 1);
        scanf("%d", &processTable[i].arrivalTime);
        
        processTable[i].executedTime = 0;
        processTable[i].burstTime = pcb->upperMemoryBound - 9 - pcb->lowerMemoryBound + 1;
        processTable[i].hasArrived = false;
        processTable[i].isComplete = false;
        processTable[i].currentQueueLevel = 0;
        processTable[i].quantumRemaining = 0;
    }
    
    // Display initial memory state
    displayMemory();
    
    // Display process table
    displayProcessTable();
    
    // Run the selected scheduling algorithm
    switch (algorithm) {
        case FCFS:
            run_fcfs_scheduler();
            break;
        case ROUND_ROBIN:
            run_rr_scheduler(RR_QUANTUM);
            break;
        case MLFQ:
            run_mlfq_scheduler();
            break;
    }
    
    // Display final memory state
    displayMemory();
    
    // Display final process table
    displayProcessTable();
    
    // Clean up resources
    cleanupResources();
    
    return 0;
}