#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>
#include <time.h>
#include <ctype.h>
#include <stdarg.h>
#include "GUI.h" 
#include "scheduler.h"

extern SchedulerGUI *gui;
bool waiting_for_input = false;
char pending_input_var[100] = "";
PCB* pending_input_process = NULL;

 // Default quantum for Round Robin

memoryWord *memory ;
int memorySize = 60;
ProcessTableEntry* processTable = NULL;
int numProcesses = 0;
Queue readyQueue = {NULL, NULL};
MLFQScheduler mlfqScheduler;
SchedulingAlgorithm algorithm = FCFS;
int currentTime = -1;
int processCount = 0;
Variable variables[MAX_VARIABLES];
int var_count = 0;
Mutex fileMutex, inputMutex, outputMutex;
PCB* currentRunningProcess = NULL;
bool simulationPaused = false;
bool simulationRunning = false;
int stepRemainingQuantum = 0;
int currentProcessIndex = -1;
int RR_QUANTUM = 4;

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
        log_message(gui, "Error: Failed to allocate memory for queue node\n");
        log_message(gui,"/n");
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

// Initialize memory
void initialMemory() {
    if (memory == NULL) {   
        memorySize = 60; 
        memory = (memoryWord*)malloc(memorySize * sizeof(memoryWord));
        if (memory == NULL) {
            fprintf(stderr, "Failed to allocate memory\n");
            log_message(gui, "Failed to allocate memory\n");
            return;
        }
    for (int i = 0; i < 60; i++) {
        memory[i].name = NULL;
        memory[i].value = NULL;
        memory[i].processID = -1;
    }
}
}

// Read program file
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
        log_message(gui, "Memory allocation failed\n");
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

// Allocate memory for process
int memoryAllocate(int processID, int IC) {
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
        }
        if (flag == true) {
            firstFree = i;
            break;
        } 
    } 
    return firstFree;  // Return the first free memory location found
}

// Free memory allocated for process
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

// Create a process
PCB* createProcess(const char* fileName) {
    char** instructions;
    int IC = readProgramFile(fileName, &instructions);
    
    if (IC <= 0) {
        printf("Failed to read program file or no instructions found\n");
        return NULL;
    }
    
    
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
    newProcess->processID = processCount+1;
    strcpy(newProcess->processState, "NEW");
    newProcess->currentPriority = 0; 
    newProcess->programCounter = p;
    newProcess->lowerMemoryBound = p;
    newProcess->upperMemoryBound = p + IC + 6 + 3-1;
    newProcess->next = NULL;
    processCount++;
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
    
    printf("Created process with ID %d from file %s\n", processCount+1, fileName);
   
    printf("PCB stored at memory locations %d to %d\n", p + IC + 3, p + IC + 3 + 5);
   
    
    return newProcess;
}

//================================ Mutex Operations ===========================

// Initialize mutex
void initMutex(Mutex* mutex, const char* name) {
    strcpy(mutex->resource, name);
    mutex->locked = 0;  
    mutex->owner = NULL;
    mutex->blockedQueue = NULL;
}

// Add process to priority-sorted blocked queue
void addToBlockedQueue(PCB** head, PCB* newProcess) {
    if (*head == NULL || (*head)->currentPriority < newProcess->currentPriority) { 
        newProcess->next = *head;
        *head = newProcess;
    } else {
        PCB* current = *head;
        while (current->next != NULL && current->next->currentPriority >= newProcess->currentPriority) {
            current = current->next;
        }
        newProcess->next = current->next;
        current->next = newProcess;
    }
}

// Remove and return highest priority process
PCB* popBlockedQueue(PCB** head) {
    if (*head == NULL) 
        return NULL;
    PCB* top = *head;
    *head = (*head)->next;
    top->next = NULL;
    return top;
}

// semWait operation
void semWait(Mutex* mutex, PCB* process) {
    if (!mutex->locked) {
        mutex->locked = 1;
        mutex->owner = process;
        printf("Process %d acquired %s\n", process->processID, mutex->resource);
   
    } else {
        printf("Process %d is BLOCKED on %s\n", process->processID, mutex->resource);
        
        strcpy(process->processState, "BLOCKED");
        addToBlockedQueue(&mutex->blockedQueue, process);
    }
}

// semSignal operation
void semSignal(Mutex* mutex, PCB* process) {
    if (mutex->owner != process) {
        printf("Process %d cannot release mutex it doesn't own.\n", process->processID);
    
        return;
    }
    
    if (mutex->blockedQueue != NULL) {
        PCB* nextProcess = popBlockedQueue(&mutex->blockedQueue);
        strcpy(nextProcess->processState, "READY");
        mutex->owner = nextProcess;
        
        for (int i = 0; i < numProcesses; i++) {
            if (processTable[i].pcb != NULL && processTable[i].pcb->processID == nextProcess->processID) {
                if (algorithm == MLFQ) {
                    int level = processTable[i].currentQueueLevel;
                    int remainingQuantum = processTable[i].quantumRemaining;
                    
                    if (remainingQuantum <= 0 && level < NUM_MLFQ_LEVELS - 1) {
                        level++;
                        remainingQuantum = mlfqScheduler.timeQuantums[level];
                    }
                    
                    processTable[i].currentQueueLevel = level;
                    processTable[i].quantumRemaining = remainingQuantum;
                    
                    enqueue(&mlfqScheduler.queues[level], nextProcess);
                } else {
                    enqueue(&readyQueue, nextProcess);
                }
                break;
            }
        }
        printf("Process %d is UNBLOCKED and acquired %s\n", nextProcess->processID, mutex->resource);
        
    } 
    else {
        mutex->locked = 0;
        mutex->owner = NULL;
        printf("%s is now FREE\n", mutex->resource);  
    }
}

//================================ Instruction Execution ===========================


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
    int start = (value[0] == '-') ? 1 : 0;
    for (int i = start; value[i] != '\0'; i++) {
        if (!isdigit(value[i])) {
            return 0;
        }
    }
    return 1;
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
    var->is_numeric = isNumeric(value);
}

// print instruction
// void print(const char* arg) {
//     Variable* var = findVariable(arg);
//     if (var != NULL) {
//         printf("%s\n", var->value);
//         int number = atoi(var->value);
//         char str[20];
//         sprintf(str, "%d", number);
//     } else {
//         printf("Error: Variable '%s' not found\n", arg);
       
//     }
// }
// print instruction
void print(const char* arg) {
    Variable* var = findVariable(arg);
    if (var != NULL) {
        // Print to console for debugging
        printf("%s\n", var->value);
        
        // Print to GUI output area
        if (gui) {
            add_output_message(gui, var->value);
        }
    } else {
        // Print error message
        printf("Error: Variable '%s' not found\n", arg);
        
        // Print error to GUI output area
        if (gui) {
            char error_msg[100];
            snprintf(error_msg, sizeof(error_msg), "Error: Variable '%s' not found", arg);
            add_output_message(gui, error_msg);
        }
    }
}
// assign instruction
// void assign(const char* varName, const char* value, PCB* currentProcess) {
//     if (strcmp(value, "input") == 0) {
//         char input[100];
//         int valid = 0;
        
//         if (varName[0] >= 'a' && varName[0] <= 'z') {
//             while (!valid) {
//                 printf("Please enter an integer value for %s: ", varName);
//                 scanf("%s", input);
//                 input[strcspn(input, "\n")] = '\0';
                
//                 valid = 1;
//                 for (int i = 0; input[i] != '\0'; i++) {
//                     if (i == 0 && input[i] == '-') continue;
//                     if (!isdigit(input[i])) {
//                         valid = 0;
//                         printf("Error: Please enter a valid integer.\n");
//                         break;
//                     }
//                 }
//             }
//         } else {
//             printf("Please enter a value for %s: ", varName);
            
//             fgets(input, sizeof(input), stdin);
//             input[strcspn(input, "\n")] = '\0';
            
//         }
        
//         setVariable(varName, input);
//     }
//     else if (strncmp(value, "readFile", 8) == 0) {
//         char filename[100];
//         sscanf(value, "readFile %s", filename);

//         Variable* fileVar = findVariable(filename);
//         const char* fname = fileVar ? fileVar->value : filename;

//         FILE* f = fopen(fname, "r");
//         if (!f) {
//             printf("Error: Cannot read file '%s'\n", fname);
           
//             return;
//         }

//         char buffer[MAX_CONTENT_LENGTH] = "";
//         fread(buffer, 1, MAX_CONTENT_LENGTH, f);
//         fclose(f);
//         buffer[strcspn(buffer, "\0")] = '\0';
//         setVariable(varName, buffer);
//     }
//     else {
//         if (isNumeric(value)) {
//             setVariable(varName, value);
//         } else {
//             Variable* srcVar = findVariable(value);
//             if (srcVar != NULL) {
//                 setVariable(varName, srcVar->value);
//             } else {
//                 printf("Error: Value '%s' is not valid\n", value);
               
//             }
//         }
//     }
    
//     // Update memory
//     if (currentProcess != NULL) {
//         int found = 0;
//         int start = currentProcess->lowerMemoryBound;
//         int end = currentProcess->upperMemoryBound - 6;
        
//         for (int i = start + (end - start - 3); i < end; i++) {
//             if ((strcmp(memory[i].name, "Variable") == 0 && strcmp(memory[i].value, "NULL") == 0) ||
//                 (memory[i].name != NULL && strcmp(memory[i].name, varName) == 0)) {
                
//                 if (memory[i].name != NULL && strcmp(memory[i].name, "Variable") == 0) {
//                     free(memory[i].name);
//                     memory[i].name = strdup(varName);
//                 }
                
//                 if (memory[i].value != NULL) {
//                     free(memory[i].value);
//                 }
                
//                 const char* valueToStore = value;
//                 if (strcmp(value, "input") == 0) {
//                     Variable* var = findVariable(varName);
//                     if (var != NULL) {
//                         valueToStore = var->value;
//                     }
//                 } else if (strncmp(value, "readFile", 8) == 0) {
//                     Variable* var = findVariable(varName);
//                     if (var != NULL) {
//                         valueToStore = var->value;
//                     }
//                 }
                
//                 memory[i].value = strdup(valueToStore);
//                 found = 1;
//                 break;
//             }
//         }
        
//         if (!found) {
//             printf("Error: No space available in process memory for variable %s\n", varName);
           
//         }
//     }
// }
// assign instruction
void assign(const char* varName, const char* value, PCB* currentProcess) {
    if (strcmp(value, "input") == 0) {
        // Set up state for input dialog
        waiting_for_input = true;
        strncpy(pending_input_var, varName, sizeof(pending_input_var) - 1);
        pending_input_var[sizeof(pending_input_var) - 1] = '\0';
        pending_input_process = currentProcess;
        
        // Pause simulation while waiting for input
        if (gui->timer_id != 0) {
            g_source_remove(gui->timer_id);
            gui->timer_id = 0;
        }
        
        // Show input dialog
        bool numeric_only = (varName[0] >= 'a' && varName[0] <= 'z');
        show_input_dialog(varName, numeric_only);
        printf("Waiting for input...\n");
        
        // Return early - the rest will be handled via the dialog callback
        return;
    }
    else if (strncmp(value, "readFile", 8) == 0) {
        char filename[100];
        sscanf(value, "readFile %s", filename);

        Variable* fileVar = findVariable(filename);
        const char* fname = fileVar ? fileVar->value : filename;

        FILE* f = fopen(fname, "r");
        if (!f) {
            printf("Error: Cannot read file '%s'\n", fname);
            log_message(gui, "Error: Cannot read file");
            return;
        }

        char buffer[MAX_CONTENT_LENGTH] = "";
        fread(buffer, 1, MAX_CONTENT_LENGTH, f);
        fclose(f);
        buffer[strcspn(buffer, "\0")] = '\0';
        setVariable(varName, buffer);
    }
    else {
        if (isNumeric(value)) {
            setVariable(varName, value);
        } else {
            Variable* srcVar = findVariable(value);
            if (srcVar != NULL) {
                setVariable(varName, srcVar->value);
            } else {
                printf("Error: Value '%s' is not valid\n", value);
                log_message(gui, "Error: Invalid value");
            }
        }
    }
    
    // Update memory
    if (currentProcess != NULL) {
        int found = 0;
        int start = currentProcess->lowerMemoryBound;
        int end = currentProcess->upperMemoryBound - 6;
        
        for (int i = start + (end - start - 3); i < end; i++) {
            if ((strcmp(memory[i].name, "Variable") == 0 && strcmp(memory[i].value, "NULL") == 0) ||
                (memory[i].name != NULL && strcmp(memory[i].name, varName) == 0)) {
                
                if (memory[i].name != NULL && strcmp(memory[i].name, "Variable") == 0) {
                    free(memory[i].name);
                    memory[i].name = strdup(varName);
                }
                
                if (memory[i].value != NULL) {
                    free(memory[i].value);
                }
                
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
            log_message(gui, "Error: No memory space for variable");
        }
    }
}

// printFromTo instruction
// void printFromTo(const char* startStr, const char* endStr) {
//     int start, end;
//     Variable* startVar = findVariable(startStr);
//     Variable* endVar = findVariable(endStr);

//     if (startVar != NULL && startVar->is_numeric) {
//         start = atoi(startVar->value);
//     } else if (isNumeric(startStr)) {
//         start = atoi(startStr);
//     } else {
//         printf("Error: Invalid start value '%s'\n", startStr);
        
//         return;
//     }

//     if (endVar != NULL && endVar->is_numeric) {
//         end = atoi(endVar->value);
//     } else if (isNumeric(endStr)) {
//         end = atoi(endStr);
//     } else {
//         printf("Error: Invalid end value '%s'\n", endStr);
       
//         return;
//     }

//     int step = (start <= end) ? 1 : -1;
//     for (int i = start; (step == 1 ? i <= end : i >= end); i += step) {
//         printf("%d ", i);
//         int number = i;
//         char str[20];
//         sprintf(str, "%d", number);
      
//     }
//     printf("\n");

// }

// // writeFile instruction
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

// readFile instruction
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
        if (gui) {
            char error_msg[100];
            snprintf(error_msg, sizeof(error_msg), "Error: Invalid start value '%s'", startStr);
            add_output_message(gui, error_msg);
        }
        return;
    }

    if (endVar != NULL && endVar->is_numeric) {
        end = atoi(endVar->value);
    } else if (isNumeric(endStr)) {
        end = atoi(endStr);
    } else {
        printf("Error: Invalid end value '%s'\n", endStr);
        if (gui) {
            char error_msg[100];
            snprintf(error_msg, sizeof(error_msg), "Error: Invalid end value '%s'", endStr);
            add_output_message(gui, error_msg);
        }
        return;
    }

    // Construct output for both console and GUI
    char output[1024] = "";
    int step = (start <= end) ? 1 : -1;
    
    for (int i = start; (step == 1 ? i <= end : i >= end); i += step) {
        char num_str[20];
        snprintf(num_str, sizeof(num_str), "%d ", i);
        strcat(output, num_str);
    }
    
    // Print to console
    printf("%s\n", output);
    
    // Print to GUI output area
    if (gui) {
        add_output_message(gui, output);
    }
}
// // Execute a single instruction
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
       printf("Error: Unknown command '%s'\n", command);
        
    }
}


//================================ Scheduling Algorithms ===========================



// Add this simulation initialization function
void initialize_simulation() {
    // Initialize memory
    initialMemory();
    
    // Initialize mutexes
    initMutex(&fileMutex, "file");
    initMutex(&inputMutex, "userInput");
    initMutex(&outputMutex, "userOutput");
    
    // Initialize ready queue
    initialize_queue(&readyQueue);
    
    // Initialize MLFQ scheduler if needed
    if (algorithm == MLFQ) {
        // Initialize with default quantum values
        mlfqScheduler.timeQuantums[0] = 1;  // Level 1: 1 time unit
        mlfqScheduler.timeQuantums[1] = 2;  // Level 2: 2 time units
        mlfqScheduler.timeQuantums[2] = 4;  // Level 3: 4 time units
        mlfqScheduler.timeQuantums[3] = RR_QUANTUM;  // Level 4: configurable
        
        // Initialize queues
        for (int i = 0; i < NUM_MLFQ_LEVELS; i++) {
            initialize_queue(&mlfqScheduler.queues[i]);
        }
        
    }
    //test
    // currentTime = -1;
    currentTime = 0;
    currentRunningProcess = NULL;
    if (processTable == NULL) {
        processTable = (ProcessTableEntry*)malloc(MAX_PROCESSES * sizeof(ProcessTableEntry));
        if (processTable == NULL) {
            fprintf(stderr, "Failed to allocate process table\n");
            log_message(gui, "Failed to allocate process table\n");
            log_message(gui,"/n");
            return;
        }

    for (int i = 0; i < MAX_PROCESSES; i++) {
        processTable[i].pcb = NULL;
        processTable[i].filename[0] = '\0';
        processTable[i].arrivalTime = 0;
        processTable[i].burstTime = 0;
        processTable[i].executedTime = 0;
        processTable[i].waitingTime = 0;
        processTable[i].priority = 0;
        processTable[i].currentQueueLevel = 0;
        processTable[i].quantumRemaining = 0;
        processTable[i].hasArrived = false;
        processTable[i].isComplete = false;
    }

    numProcesses = 0;
    //currentTime = -1;//test
    processCount = 0;
    var_count = 0;
    currentRunningProcess = NULL;
}
}

// This function checks for and handles processes that have arrived at the current time
void check_for_process_arrivals(int currentTime) {
    int numArrivingProcesses = 0;
    PCB* arrivingProcesses[MAX_PROCESSES];
    int priorityValues[MAX_PROCESSES];
    
    printf("Checking for process arrivals at time %d\n", currentTime);
    
    // Check all processes in process table
    for (int i = 0; i < numProcesses; i++) {
        if (!processTable[i].hasArrived && processTable[i].arrivalTime == currentTime) {
            // Process arrival time has been reached
            
            // Create PCB if not already created
            if (processTable[i].pcb == NULL) {
                printf("Creating process for entry %d\n", i);
                PCB* pcb = createProcess(processTable[i].filename);
                
                if (pcb == NULL) {
                    printf("Failed to create process %d at arrival time\n", i);
                    continue; // Skip this process
                }
                
                // Set PCB in process table
                processTable[i].pcb = pcb;
                
                // Calculate burst time now that we have memory bounds
                int instructionCount = pcb->upperMemoryBound - 
                                      pcb->lowerMemoryBound + 1 - 9;
                processTable[i].burstTime = instructionCount;
            }
            
            // Mark as arrived AFTER successful creation
            processTable[i].hasArrived = true;
            
            printf("Time %d: PID %d arrived with priority %d\n", 
                currentTime, processTable[i].pcb->processID, processTable[i].pcb->currentPriority);
            
            // Set the process state to READY
            strcpy(processTable[i].pcb->processState, "READY");
            
            // Add to our temporary array - use the current count as index, then increment
            arrivingProcesses[numArrivingProcesses] = processTable[i].pcb; 
            priorityValues[numArrivingProcesses] = processTable[i].pcb->currentPriority;
            numArrivingProcesses++;
        }  
    }
    
    // Sort the arriving processes by priority
    for (int i = 0; i < numArrivingProcesses - 1; i++) {
        for (int j = 0; j < numArrivingProcesses - i - 1; j++) {
            if (priorityValues[j] < priorityValues[j + 1]) {
                // Swap priorities
                int tempPriority = priorityValues[j];
                priorityValues[j] = priorityValues[j + 1];
                priorityValues[j + 1] = tempPriority;
                
                // Swap processes
                PCB* tempPCB = arrivingProcesses[j];
                arrivingProcesses[j] = arrivingProcesses[j + 1];
                arrivingProcesses[j + 1] = tempPCB;
            }
        }
    }
    
    // Enqueue the processes in priority order
    if (numArrivingProcesses > 0) {
        printf("Enqueueing %d processes in priority order:\n", numArrivingProcesses);
        
        for (int i = 0; i < numArrivingProcesses; i++) {
            // Verify the process pointer is valid before enqueueing
            if (arrivingProcesses[i] == NULL) {
                printf("Error: NULL process at index %d\n", i);
                continue;
            }
            
            if (algorithm == MLFQ) {
                // For MLFQ, add to the highest priority queue (level 0)
                enqueue(&mlfqScheduler.queues[0], arrivingProcesses[i]);
                
                // Update the process table entry
                for (int j = 0; j < numProcesses; j++) {
                    if (processTable[j].pcb && 
                        processTable[j].pcb->processID == arrivingProcesses[i]->processID) {
                        processTable[j].currentQueueLevel = 0;
                        processTable[j].quantumRemaining = mlfqScheduler.timeQuantums[0];
                        break;
                    }
                }
                
                printf("  Process %d (Priority %d) added to highest priority queue (Level 1, Queue 0)\n", 
                       arrivingProcesses[i]->processID, priorityValues[i]);
            } else {
                // For FCFS and RR, add to the main ready queue
                enqueue(&readyQueue, arrivingProcesses[i]);
                printf("  Process %d (Priority %d) added to ready queue\n", 
                       arrivingProcesses[i]->processID, priorityValues[i]);
            }
        }
    }
}

bool all_processes_complete() {
    for (int i = 0; i < numProcesses; i++) {
        if (!processTable[i].isComplete) {
            return false;
        }
    }
    return true;
}

void run_fcfs_scheduler() {
    printf("Starting FCFS scheduler\n");
    
    currentTime = -1;
    
    while (!all_processes_complete()) {
        // Check for new arrivals
        check_for_process_arrivals(currentTime);
        
        // Get next process from ready queue
        PCB* currentProcess = dequeue(&readyQueue);
        
        if (currentProcess != NULL) {
            strcpy(currentProcess->processState, "RUNNING");
            printf("Time %d: Program %d is now running\n", currentTime, currentProcess->processID);
            memory[currentProcess->upperMemoryBound-4].value = currentProcess->processState; // update process state in memory

            
            // Find the process entry to determine burst time
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
            while (remainingInstructions > 0)  {
                 
                
                int memoryIndex = currentProcess->programCounter;
                
                // Execute the instruction
                printf("Time %d: Executing process %d, instruction: %s\n", 
                       currentTime, currentProcess->processID, memory[memoryIndex].value);
                
                execute(memory[memoryIndex].value, currentProcess); // Execute the instruction      
                // Update program counter
                currentProcess->programCounter++;
                // Create a temporary buffer for the string conversion
                 char tempBuffer[20];
                // Convert the integer to a string
                sprintf(tempBuffer, "%d", currentProcess->programCounter);
                // Free the old value to prevent memory leaks
                if (memory[currentProcess->upperMemoryBound-2].value != NULL) {
                    free(memory[currentProcess->upperMemoryBound-2].value);
                    }
                // Allocate and store the new string value
                memory[currentProcess->upperMemoryBound-2].value = strdup(tempBuffer); // update pc in memory
              // displayMemory(); // Display memory after each instruction execution
             
                
                // Advance time
                currentTime++;
                sleep(1); // Simulate time passing
                processTable[processIndex].executedTime++;
                
                // Decrement remaining instructions
                remainingInstructions--;

                for(int i = 0; i < numProcesses; i++) {
                    if (processTable[i].hasArrived && !processTable[i].isComplete &&
                        processTable[i].pcb->processID != currentProcess->processID) {
                        processTable[i].waitingTime++;
                    }
                }
                
                
                // Check for new arrivals after each time unit
                check_for_process_arrivals(currentTime);
            }
            
            // Process is now complete
            printf("Process %d has completed execution at time %d\n", currentProcess->processID, currentTime);
            strcpy(currentProcess->processState, "TERMINATED");
            memory[currentProcess->upperMemoryBound-4].value = currentProcess->processState; // update process state in memory
            
            // Mark process as complete in process table
            if (processIndex != -1) {
                processTable[processIndex].isComplete = true;
            }
        } else {
            printf("Time %d: CPU idle - no process in ready queue\n", currentTime);
            // Advance time if no process is available
            currentTime++;
            sleep(1);
        }
    }
    
    printf("All processes have completed execution. Total time: %d\n", currentTime);
}

// Execute one step of Round Robin algorithm
void run_rr_scheduler(int timeQuantum) {
    int currentTime = -1;
    PCB* currentProcess = NULL;
    int remainingQuantum = 0;
    
    printf("\nStarting Round Robin Scheduler with Time Quantum = %d\n", timeQuantum);
    
    while (!all_processes_complete()) {
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
        
        // Update waiting time for all processes except the current one
        for (int i = 0; i < numProcesses; i++) {
            if (processTable[i].hasArrived && !processTable[i].isComplete &&
                (currentProcess == NULL || processTable[i].pcb->processID != currentProcess->processID)) {
                processTable[i].waitingTime++;
            }
        }
        
        if (currentProcess != NULL) {
            int processIndex = -1;
            for (int i = 0; i < numProcesses; i++) {
                if (processTable[i].pcb != NULL && processTable[i].pcb->processID == currentProcess->processID) {
                    processIndex = i;
                    break;
                }
            }
            
            if (processIndex != -1) {  // Make sure we found the process
                // Calculate the position of the last instruction
                int lastInstructionPosition = currentProcess->upperMemoryBound - 9;
                
                // Execute one instruction only if we haven't reached the end
                if (currentProcess->programCounter <= lastInstructionPosition) {
                    int memoryIndex = currentProcess->programCounter;
                    
                    printf("Time %d: Executing process %d, instruction: %s (Quantum remaining: %d)\n", 
                           currentTime, currentProcess->processID, memory[memoryIndex].value, remainingQuantum);
                    
                    execute(memory[memoryIndex].value, currentProcess);                
                    currentProcess->programCounter++;
                    processTable[processIndex].executedTime++;
                    
                    // Create a temporary buffer for the string conversion
                    char tempBuffer[20];
                    // Convert the integer to a string
                    sprintf(tempBuffer, "%d", currentProcess->programCounter);
                    // Free the old value to prevent memory leaks
                    if (memory[currentProcess->upperMemoryBound-2].value != NULL) {
                        free(memory[currentProcess->upperMemoryBound-2].value);
                    }
                    // Allocate and store the new string value
                    memory[currentProcess->upperMemoryBound-2].value = strdup(tempBuffer);
                    
                    remainingQuantum--;
                    
                    // Check if process was blocked
                    if (strcmp(currentProcess->processState, "BLOCKED") == 0) {
                        printf("Time %d: Process %d was blocked, moving out of CPU\n",
                               currentTime, currentProcess->processID);
                        currentProcess = NULL;  
                    }
                    // Check if process has completed all its instructions
                    else if (currentProcess->programCounter > lastInstructionPosition) {
                        printf("Process %d has completed execution at time %d\n", currentProcess->processID, currentTime + 1);
                        strcpy(currentProcess->processState, "TERMINATED");
                        processTable[processIndex].isComplete = true;
                        currentProcess = NULL;
                    }
                    // Check if time quantum has expired
                    else if (remainingQuantum <= 0) {
                        printf("Time %d: Process %d time quantum expired, moving to ready queue\n", 
                               currentTime, currentProcess->processID);
                        strcpy(currentProcess->processState, "READY");
                        enqueue(&readyQueue, currentProcess);
                        currentProcess = NULL;
                    }
                } else {
                    // Process completed all instructions
                    printf("Process %d has completed execution at time %d\n", currentProcess->processID, currentTime);
                    strcpy(currentProcess->processState, "TERMINATED");
                    processTable[processIndex].isComplete = true;
                    currentProcess = NULL;
                }
            } else {
                // Something went wrong - couldn't find the process in the table
                printf("Error: Current process %d not found in process table\n", currentProcess->processID);
                currentProcess = NULL;
            }
        } else {
            printf("Time %d: CPU idle - no process in ready queue\n", currentTime);
        }
        
        // Advance time
        currentTime++;
        sleep(1); // Simulate idle time
    }
    
    printf("All processes have completed execution. Total time: %d\n", currentTime);
}
    

// Execute one step of MLFQ algorithm
void run_mlfq_scheduler() {
    printf("\nStarting Multi-Level Feedback Queue Scheduler\n");
    
    int currentTime = -1;
    PCB* currentProcess = NULL;
    int currentLevel = 0;
    int remainingQuantum = 0;
    
    while (!all_processes_complete()) {
        // Check for new arrivals
        check_for_process_arrivals(currentTime);
        
        // If no process is currently running, get one from the highest non-empty queue
        if (currentProcess == NULL) {
            for (int level = 0; level < NUM_MLFQ_LEVELS; level++) {
                currentProcess = dequeue(&mlfqScheduler.queues[level]);
                if (currentProcess != NULL) {
                    currentLevel = level;
                    
                    // Find the process in the process table to get its remaining quantum
                    for (int i = 0; i < numProcesses; i++) {
                        if (processTable[i].pcb != NULL && processTable[i].pcb->processID == currentProcess->processID) {
                            remainingQuantum = processTable[i].quantumRemaining;
                            break;
                        }
                    }
                    
                    // Update process state
                    strcpy(currentProcess->processState, "RUNNING");
                    if (memory[currentProcess->upperMemoryBound-4].value != NULL) {
                        free(memory[currentProcess->upperMemoryBound-4].value);
                    }
                    memory[currentProcess->upperMemoryBound-4].value = strdup("RUNNING");
                    
                    printf("Time %d: Process %d from Level %d is now running (Quantum: %d)\n", 
                        currentTime, currentProcess->processID, level + 1, remainingQuantum);
                    break;
                }
            }
        }
        
        // Update waiting time for all ready processes
        for (int i = 0; i < numProcesses; i++) {
            if (processTable[i].hasArrived && !processTable[i].isComplete &&
                (currentProcess == NULL || processTable[i].pcb->processID != currentProcess->processID)) {
                // Only increment waiting time for ready processes, not blocked ones
                if (strcmp(processTable[i].pcb->processState, "READY") == 0) {
                    processTable[i].waitingTime++;
                }
            }
        }
        
        if (currentProcess != NULL) {
            int processIndex = -1;
            for (int i = 0; i < numProcesses; i++) {
                if (processTable[i].pcb != NULL && processTable[i].pcb->processID == currentProcess->processID) {
                    processIndex = i;
                    break;
                }
            }
            
            if (processIndex != -1) {
                // Execute one instruction
                int lastInstructionPosition = currentProcess->upperMemoryBound - 9;
                if (currentProcess->programCounter <= lastInstructionPosition) {
                    int memoryIndex = currentProcess->programCounter;
                    
                    printf("Time %d: Executing process %d, instruction: %s (Level: %d, Quantum remaining: %d)\n", 
                        currentTime, currentProcess->processID, memory[memoryIndex].value, 
                        currentLevel + 1, remainingQuantum);
                    
                    execute(memory[memoryIndex].value, currentProcess);
                    
                    // Update program counter
                    currentProcess->programCounter++;
                    // Update program counter in memory
                    char tempBuffer[20];
                    sprintf(tempBuffer, "%d", currentProcess->programCounter);
                    if (memory[currentProcess->upperMemoryBound-2].value != NULL) {
                        free(memory[currentProcess->upperMemoryBound-2].value);
                    }
                    memory[currentProcess->upperMemoryBound-2].value = strdup(tempBuffer);
                    
                    processTable[processIndex].executedTime++;
                    remainingQuantum--;
                    processTable[processIndex].quantumRemaining = remainingQuantum;
                    
                    // Check if process was blocked after executing the instruction
                    if (strcmp(currentProcess->processState, "BLOCKED") == 0) {
                        printf("Time %d: Process %d was blocked, moving out of CPU\n",
                            currentTime, currentProcess->processID);
                        if (memory[currentProcess->upperMemoryBound-4].value != NULL) {
                            free(memory[currentProcess->upperMemoryBound-4].value);
                        }
                        memory[currentProcess->upperMemoryBound-4].value = strdup("BLOCKED");
                        
                        // Save the current level and quantum in the process table
                        processTable[processIndex].currentQueueLevel = currentLevel;
                        
                        currentProcess = NULL;
                        // The blocked process is already in the mutex's blocked queue
                        // so we don't need to enqueue it anywhere else
                    }
                    // Check if process has completed
                    else if (currentProcess->programCounter > lastInstructionPosition) {
                        printf("Process %d has completed execution at time %d\n", 
                            currentProcess->processID, currentTime + 1);
                        strcpy(currentProcess->processState, "TERMINATED");
                        if (memory[currentProcess->upperMemoryBound-4].value != NULL) {
                            free(memory[currentProcess->upperMemoryBound-4].value);
                        }
                        memory[currentProcess->upperMemoryBound-4].value = strdup("TERMINATED");
                        processTable[processIndex].isComplete = true;
                        memorydeallocate(currentProcess->processID - 1); // Adjust for the off-by-one error
                        free(currentProcess);
                        currentProcess = NULL;
                    }
                    // Check if time quantum has expired
                    else if (remainingQuantum <= 0) {
                        // For level 4 (lowest), use Round Robin (keep in same queue)
                        if (currentLevel == NUM_MLFQ_LEVELS - 1) {
                            printf("Time %d: Process %d time quantum expired in Level 4 (RR), returning to queue\n", 
                                currentTime + 1, currentProcess->processID);
                            strcpy(currentProcess->processState, "READY");
                            if (memory[currentProcess->upperMemoryBound-4].value != NULL) {
                                free(memory[currentProcess->upperMemoryBound-4].value);
                            }
                            memory[currentProcess->upperMemoryBound-4].value = strdup("READY");
                            
                            // Reset quantum for RR
                            remainingQuantum = mlfqScheduler.timeQuantums[currentLevel];
                            processTable[processIndex].quantumRemaining = remainingQuantum;
                            enqueue(&mlfqScheduler.queues[currentLevel], currentProcess);
                        }
                        // For levels 1-3, move to lower level when quantum expires
                        else {
                            int newLevel = currentLevel + 1;
                            printf("Time %d: Process %d time quantum expired in Level %d, moving to Level %d\n", 
                                currentTime + 1, currentProcess->processID, currentLevel + 1, newLevel + 1);
                            strcpy(currentProcess->processState, "READY");
                            if (memory[currentProcess->upperMemoryBound-4].value != NULL) {
                                free(memory[currentProcess->upperMemoryBound-4].value);
                            }
                            memory[currentProcess->upperMemoryBound-4].value = strdup("READY");
                            
                            // Update process table with new level and reset quantum
                            processTable[processIndex].currentQueueLevel = newLevel;
                            remainingQuantum = mlfqScheduler.timeQuantums[newLevel];
                            processTable[processIndex].quantumRemaining = remainingQuantum;
                            enqueue(&mlfqScheduler.queues[newLevel], currentProcess);
                        }
                        currentProcess = NULL;
                    }
                } else {
                    // Process completed all instructions but somehow we're still here
                    printf("Process %d has completed execution at time %d\n", 
                        currentProcess->processID, currentTime);
                    strcpy(currentProcess->processState, "TERMINATED");
                    if (memory[currentProcess->upperMemoryBound-4].value != NULL) {
                        free(memory[currentProcess->upperMemoryBound-4].value);
                    }
                    memory[currentProcess->upperMemoryBound-4].value = strdup("TERMINATED");
                    processTable[processIndex].isComplete = true;
                    memorydeallocate(currentProcess->processID - 1); // Adjust for the off-by-one error
                    free(currentProcess);
                    currentProcess = NULL;
                }
            } else {
                // Process not found in the table
                printf("Error: Current process %d not found in process table\n", currentProcess->processID);
                currentProcess = NULL;
            }
        } else {
            // Check if all queues are empty but there are still blocked processes
            bool allQueuesEmpty = true;
            for (int level = 0; level < NUM_MLFQ_LEVELS; level++) {
                if (!is_queue_empty(&mlfqScheduler.queues[level])) {
                    allQueuesEmpty = false;
                    break;
                }
            }
            
            // Check if we have blocked processes but no ready processes
            if (allQueuesEmpty) {
                bool hasBlockedProcess = false;
                for (int i = 0; i < numProcesses; i++) {
                    if (processTable[i].hasArrived && !processTable[i].isComplete && 
                        strcmp(processTable[i].pcb->processState, "BLOCKED") == 0) {
                        hasBlockedProcess = true;
                        break;
                    }
                }
                
                if (hasBlockedProcess) {
                    printf("Time %d: CPU idle - all processes blocked or finished\n", currentTime);
                } else {
                    printf("Time %d: CPU idle - no process in any queue\n", currentTime);
                }
            }
        }
        
        // Advance time 
        currentTime++;
        sleep(1); // Simulate time passing
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

//-----------------------------Step Execution-----------------------------------------

// Add these functions before execute_step()

// Execute a single step of FCFS scheduling algorithm
void run_fcfs_step() {
    // Check for new arrivals at current time
    printf("checking before process arrivals\n");
    check_for_process_arrivals(currentTime+1);
    printf("process arrivals checked\n");      
    // If no process is running, try to get one from the ready queue
    if (currentRunningProcess == NULL) {
        printf("dequeueing process\n");
        currentRunningProcess = dequeue(&readyQueue);
        printf("process dequeued\n");
        if (currentRunningProcess != NULL) {
            // Found a process to run
            printf("Found a process to run\n");
            strcpy(currentRunningProcess->processState, "RUNNING");
            printf("Time %d: Process %d is now running\n", 
                  currentTime, currentRunningProcess->processID);
            printf("before updating memory\n");
            // Update process state in memory
            if (memory[currentRunningProcess->upperMemoryBound-4].value != NULL) {
                free(memory[currentRunningProcess->upperMemoryBound-4].value);
            }
            memory[currentRunningProcess->upperMemoryBound-4].value = strdup("RUNNING");
            printf("after updating memory\n");
            // Find the process in the process table
            currentProcessIndex = -1;
            for (int i = 0; i < numProcesses; i++) {
                if (processTable[i].pcb != NULL && 
                    processTable[i].pcb->processID == currentRunningProcess->processID) {
                    currentProcessIndex = i;
                    break;
                }
            }
            
            // Log message about process starting
            char message[100];
            snprintf(message, sizeof(message), "Process %d started execution", 
                    currentRunningProcess->processID);
            log_message(gui, message);
            
            // Don't execute an instruction yet when we just scheduled this process
            return;
        }
        else {
            // No process available to run
            printf("Time %d: CPU idle - no process in ready queue\n", currentTime);
            log_message(gui, "CPU idle - no process in ready queue");
            return;
        }
    }
    
    // At this point, we have a current running process
    if (currentProcessIndex != -1) {
        // Execute one instruction
        int lastInstructionPosition = currentRunningProcess->upperMemoryBound - 9;
        
        if (currentRunningProcess->programCounter <= lastInstructionPosition) {
            int memoryIndex = currentRunningProcess->programCounter;
            printf("getting the pc, before evaluating\n");  
            // Execute the instruction
            printf("Time %d: Executing process %d, instruction: %s\n",
                  currentTime, currentRunningProcess->processID, memory[memoryIndex].value);
            
            // Log the instruction being executed
            char message[200];
            snprintf(message, sizeof(message), "Time %d: Process %d executing: %s", 
                    currentTime, currentRunningProcess->processID, memory[memoryIndex].value);
            log_message(gui, message);
            
            // Actually execute the instruction
            execute(memory[memoryIndex].value, currentRunningProcess);
            printf("after executing the instruction\n");    
            // Update program counter
            currentRunningProcess->programCounter++;
            
            // Update program counter in memory
            char tempBuffer[20];
            sprintf(tempBuffer, "%d", currentRunningProcess->programCounter);
            if (memory[currentRunningProcess->upperMemoryBound-2].value != NULL) {
                free(memory[currentRunningProcess->upperMemoryBound-2].value);
            }
            memory[currentRunningProcess->upperMemoryBound-2].value = strdup(tempBuffer);
            printf("after updating the pc in memory\n");
            // Update executed time for the process
            processTable[currentProcessIndex].executedTime++;
            
            // Update waiting time for all other ready processes
            for (int i = 0; i < numProcesses; i++) {
                if (processTable[i].hasArrived && !processTable[i].isComplete &&
                    processTable[i].pcb->processID != currentRunningProcess->processID &&
                    strcmp(processTable[i].pcb->processState, "READY") == 0) {
                    processTable[i].waitingTime++;
                }
            }
            printf("after updating waiting time\n");    
            // Check if process was blocked after executing the instruction
            if (strcmp(currentRunningProcess->processState, "BLOCKED") == 0) {
                printf("Time %d: Process %d was blocked, moving out of CPU\n",
                      currentTime, currentRunningProcess->processID);
                log_message(gui, "Process blocked, moving out of CPU");
                currentRunningProcess = NULL;
                printf("after setting current running process to blocking\n");
            }
            // Check if process has completed all instructions
            else if (currentRunningProcess->programCounter > lastInstructionPosition) {
                printf("Process %d has completed execution at time %d\n", 
                      currentRunningProcess->processID, currentTime);
                
                strcpy(currentRunningProcess->processState, "TERMINATED");
                if (memory[currentRunningProcess->upperMemoryBound-4].value != NULL) {
                    free(memory[currentRunningProcess->upperMemoryBound-4].value);
                }
                memory[currentRunningProcess->upperMemoryBound-4].value = strdup("TERMINATED");
                
                // Mark process as complete in process table
                processTable[currentProcessIndex].isComplete = true;
                
                // Log message about process completion
                char message[100];
                snprintf(message, sizeof(message), "Process %d completed execution", 
                        currentRunningProcess->processID);
                log_message(gui, message);
                
                // Clear the current process
                currentRunningProcess = NULL;
                printf("after setting current running process to null\n");
            }
        }
    }
}

// Execute a single step of Round Robin scheduling algorithm
void run_rr_step() {
    // Check for new arrivals
    check_for_process_arrivals(currentTime);
    
    // If no process is running, try to get one from the ready queue
    if (currentRunningProcess == NULL) {
        currentRunningProcess = dequeue(&readyQueue);
        
        if (currentRunningProcess != NULL) {
            strcpy(currentRunningProcess->processState, "RUNNING");
            printf("Time %d: Process %d is now running\n", 
                  currentTime, currentRunningProcess->processID);
            
            if (memory[currentRunningProcess->upperMemoryBound-4].value != NULL) {
                free(memory[currentRunningProcess->upperMemoryBound-4].value);
            }
            memory[currentRunningProcess->upperMemoryBound-4].value = strdup("RUNNING");
            
            // Reset quantum for this process
            stepRemainingQuantum = RR_QUANTUM;
            
            // Find the process in the process table
            currentProcessIndex = -1;
            for (int i = 0; i < numProcesses; i++) {
                if (processTable[i].pcb != NULL && 
                    processTable[i].pcb->processID == currentRunningProcess->processID) {
                    currentProcessIndex = i;
                    break;
                }
            }
            
            // Log the process starting
            char message[100];
            snprintf(message, sizeof(message), "Process %d started execution (Quantum: %d)", 
                    currentRunningProcess->processID, stepRemainingQuantum);
            log_message(gui, message);
            
            // Don't execute an instruction yet when we just scheduled this process
            return;
        }
        else {
            // No process available to run
            printf("Time %d: CPU idle - no process in ready queue\n", currentTime);
            log_message(gui, "CPU idle - no process in ready queue");
            return;
        }
    }
    
    // At this point, we have a current running process
    if (currentProcessIndex != -1) {
        // Calculate the position of the last instruction
        int lastInstructionPosition = currentRunningProcess->upperMemoryBound - 9;
        
        // Execute one instruction only if we haven't reached the end
        if (currentRunningProcess->programCounter <= lastInstructionPosition) {
            int memoryIndex = currentRunningProcess->programCounter;
            
            printf("Time %d: Executing process %d, instruction: %s (Quantum remaining: %d)\n", 
                   currentTime, currentRunningProcess->processID, memory[memoryIndex].value, 
                   stepRemainingQuantum);
            
            // Log the instruction being executed
            char message[200];
            snprintf(message, sizeof(message), "Time %d: Process %d executing: %s (Quantum: %d)", 
                    currentTime, currentRunningProcess->processID, memory[memoryIndex].value,
                    stepRemainingQuantum);
            log_message(gui, message);
                    
            // Execute the instruction
            execute(memory[memoryIndex].value, currentRunningProcess);
            
            // Update program counter
            currentRunningProcess->programCounter++;
            
            // Update program counter in memory
            char tempBuffer[20];
            sprintf(tempBuffer, "%d", currentRunningProcess->programCounter);
            if (memory[currentRunningProcess->upperMemoryBound-2].value != NULL) {
                free(memory[currentRunningProcess->upperMemoryBound-2].value);
            }
            memory[currentRunningProcess->upperMemoryBound-2].value = strdup(tempBuffer);
            
            // Update executed time for the process
            processTable[currentProcessIndex].executedTime++;
            
            // Update waiting time for all other ready processes
            for (int i = 0; i < numProcesses; i++) {
                if (processTable[i].hasArrived && !processTable[i].isComplete &&
                    processTable[i].pcb->processID != currentRunningProcess->processID &&
                    strcmp(processTable[i].pcb->processState, "READY") == 0) {
                    processTable[i].waitingTime++;
                }
            }
            
            // Decrement the remaining quantum
            stepRemainingQuantum--;
            
            // Check if process was blocked after executing the instruction
            if (strcmp(currentRunningProcess->processState, "BLOCKED") == 0) {
                printf("Time %d: Process %d was blocked, moving out of CPU\n",
                       currentTime, currentRunningProcess->processID);
                log_message(gui, "Process blocked, moving out of CPU");
                currentRunningProcess = NULL;
            }
            // Check if process has completed all instructions
            else if (currentRunningProcess->programCounter > lastInstructionPosition) {
                printf("Process %d has completed execution at time %d\n", 
                       currentRunningProcess->processID, currentTime);
                
                strcpy(currentRunningProcess->processState, "TERMINATED");
                if (memory[currentRunningProcess->upperMemoryBound-4].value != NULL) {
                    free(memory[currentRunningProcess->upperMemoryBound-4].value);
                }
                memory[currentRunningProcess->upperMemoryBound-4].value = strdup("TERMINATED");
                
                // Mark process as complete in process table
                processTable[currentProcessIndex].isComplete = true;
                
                // Log message about process completion
                char message[100];
                snprintf(message, sizeof(message), "Process %d completed execution", 
                        currentRunningProcess->processID);
                log_message(gui, message);
                
                // Clear the current process
                currentRunningProcess = NULL;
            }
            // Check if time quantum has expired
            else if (stepRemainingQuantum <= 0) {
                printf("Time %d: Process %d time quantum expired, moving to ready queue\n", 
                       currentTime, currentRunningProcess->processID);
                
                strcpy(currentRunningProcess->processState, "READY");
                if (memory[currentRunningProcess->upperMemoryBound-4].value != NULL) {
                    free(memory[currentRunningProcess->upperMemoryBound-4].value);
                }
                memory[currentRunningProcess->upperMemoryBound-4].value = strdup("READY");
                
                // Log message about quantum expiration
                char message[100];
                snprintf(message, sizeof(message), "Process %d quantum expired, returning to ready queue", 
                        currentRunningProcess->processID);
                log_message(gui, message);
                
                // Add the process back to the ready queue
                enqueue(&readyQueue, currentRunningProcess);
                
                // Clear the current process
                currentRunningProcess = NULL;
            }
        }
    }
}

// Execute a single step of MLFQ scheduling algorithm
// void run_mlfq_step() {
//     // Check for new arrivals
//     check_for_process_arrivals(currentTime);
    
//     // If no process is currently running, get one from the highest non-empty queue
//     if (currentRunningProcess == NULL) {
//         for (int level = 0; level < NUM_MLFQ_LEVELS; level++) {
//             currentRunningProcess = dequeue(&mlfqScheduler.queues[level]);
//             if (currentRunningProcess != NULL) {
//                 // Found a process to run
//                 strcpy(currentRunningProcess->processState, "RUNNING");
                
//                 if (memory[currentRunningProcess->upperMemoryBound-4].value != NULL) {
//                     free(memory[currentRunningProcess->upperMemoryBound-4].value);
//                 }
//                 memory[currentRunningProcess->upperMemoryBound-4].value = strdup("RUNNING");
                
//                 // Find the process in the process table
//                 currentProcessIndex = -1;
//                 for (int i = 0; i < numProcesses; i++) {
//                     if (processTable[i].pcb != NULL && 
//                         processTable[i].pcb->processID == currentRunningProcess->processID) {
//                         currentProcessIndex = i;
                        
//                         // Set current level and quantum
//                         processTable[i].currentQueueLevel = level;
//                         stepRemainingQuantum = mlfqScheduler.timeQuantums[level];
//                         break;
//                     }
//                 }
                
//                 printf("Time %d: Process %d from Level %d is now running (Quantum: %d)\n", 
//                        currentTime, currentRunningProcess->processID, level + 1, stepRemainingQuantum);
                
//                 // Log the process starting
//                 char message[150];
//                 snprintf(message, sizeof(message), 
//                          "Process %d from Level %d started execution (Quantum: %d)", 
//                          currentRunningProcess->processID, level + 1, stepRemainingQuantum);
//                 log_message(gui, message);
                
//                 // Don't execute an instruction yet when we just scheduled this process
//                 break;
//             }
//         }
        
//         if (currentRunningProcess == NULL) {
//             // No process available to run in any queue
//             printf("Time %d: CPU idle - no process in any queue\n", currentTime);
//             log_message(gui, "CPU idle - no process in any queue");
//             return;
//         }
//     }
    
//     // At this point, we have a current running process
//     if (currentProcessIndex != -1) {
//         int currentLevel = processTable[currentProcessIndex].currentQueueLevel;
        
//         // Execute one instruction
//         int lastInstructionPosition = currentRunningProcess->upperMemoryBound - 9;
        
//         if (currentRunningProcess->programCounter <= lastInstructionPosition) {
//             int memoryIndex = currentRunningProcess->programCounter;
            
//             printf("Time %d: Executing process %d, instruction: %s (Level: %d, Quantum remaining: %d)\n", 
//                    currentTime, currentRunningProcess->processID, memory[memoryIndex].value, 
//                    currentLevel + 1, stepRemainingQuantum);
            
//             // Log the instruction being executed
//             char message[200];
//             snprintf(message, sizeof(message), 
//                      "Time %d: Process %d executing: %s (Level: %d, Quantum: %d)", 
//                      currentTime, currentRunningProcess->processID, memory[memoryIndex].value,
//                      currentLevel + 1, stepRemainingQuantum);
//             log_message(gui, message);
            
//             // Execute the instruction
//             execute(memory[memoryIndex].value, currentRunningProcess);
            
//             // Update program counter
//             currentRunningProcess->programCounter++;
            
//             // Update program counter in memory
//             char tempBuffer[20];
//             sprintf(tempBuffer, "%d", currentRunningProcess->programCounter);
//             if (memory[currentRunningProcess->upperMemoryBound-2].value != NULL) {
//                 free(memory[currentRunningProcess->upperMemoryBound-2].value);
//             }
//             memory[currentRunningProcess->upperMemoryBound-2].value = strdup(tempBuffer);
            
//             // Update executed time for the process
//             processTable[currentProcessIndex].executedTime++;
            
//             // Update quantum remaining
//             stepRemainingQuantum--;
//             processTable[currentProcessIndex].quantumRemaining = stepRemainingQuantum;
            
//             // Update waiting time for all other ready processes
//             for (int i = 0; i < numProcesses; i++) {
//                 if (processTable[i].hasArrived && !processTable[i].isComplete &&
//                     processTable[i].pcb->processID != currentRunningProcess->processID &&
//                     strcmp(processTable[i].pcb->processState, "READY") == 0) {
//                     processTable[i].waitingTime++;
//                 }
//             }
            
//             // Check if process was blocked after executing the instruction
//             if (strcmp(currentRunningProcess->processState, "BLOCKED") == 0) {
//                 printf("Time %d: Process %d was blocked, moving out of CPU\n",
//                        currentTime, currentRunningProcess->processID);
                
//                 if (memory[currentRunningProcess->upperMemoryBound-4].value != NULL) {
//                     free(memory[currentRunningProcess->upperMemoryBound-4].value);
//                 }
//                 memory[currentRunningProcess->upperMemoryBound-4].value = strdup("BLOCKED");
                
//                 log_message(gui, "Process blocked, moving out of CPU");
//                 currentRunningProcess = NULL;
//             }
//             // Check if process has completed all instructions
//             else if (currentRunningProcess->programCounter > lastInstructionPosition) {
//                 printf("Process %d has completed execution at time %d\n", 
//                        currentRunningProcess->processID, currentTime);
                
//                 strcpy(currentRunningProcess->processState, "TERMINATED");
//                 if (memory[currentRunningProcess->upperMemoryBound-4].value != NULL) {
//                     free(memory[currentRunningProcess->upperMemoryBound-4].value);
//                 }
//                 memory[currentRunningProcess->upperMemoryBound-4].value = strdup("TERMINATED");
                
//                 // Mark process as complete in process table
//                 processTable[currentProcessIndex].isComplete = true;
                
//                 // Log message about process completion
//                 char message[100];
//                 snprintf(message, sizeof(message), "Process %d completed execution", 
//                         currentRunningProcess->processID);
//                 log_message(gui, message);
                
//                 // Clear the current process
//                 currentRunningProcess = NULL;
//             }
//             // Check if time quantum has expired
//             else if (stepRemainingQuantum <= 0) {
//                 // For last level (lowest priority), use Round Robin (stay in same queue)
//                 if (currentLevel == NUM_MLFQ_LEVELS - 1) {
//                     printf("Time %d: Process %d time quantum expired in Level %d (RR), returning to queue\n", 
//                            currentTime, currentRunningProcess->processID, currentLevel + 1);
                    
//                     strcpy(currentRunningProcess->processState, "READY");
//                     if (memory[currentRunningProcess->upperMemoryBound-4].value != NULL) {
//                         free(memory[currentRunningProcess->upperMemoryBound-4].value);
//                     }
//                     memory[currentRunningProcess->upperMemoryBound-4].value = strdup("READY");
                    
//                     // Reset quantum for RR
                    
//                     stepRemainingQuantum = mlfqScheduler.timeQuantums[currentLevel];
//                     processTable[currentProcessIndex].quantumRemaining = stepRemainingQuantum;
                    
//                     // Log message about quantum expiration
//                     char message[150];
//                     snprintf(message, sizeof(message), 
//                              "Process %d quantum expired in Level %d (RR), returning to same queue", 
//                              currentRunningProcess->processID, currentLevel + 1);
//                     log_message(gui, message);
                    
//                     // Add back to same queue
//                     enqueue(&mlfqScheduler.queues[currentLevel], currentRunningProcess);
//                 }
//                 // For other levels, demote to next lower level
//                 else {
//                     int newLevel = currentLevel + 1;
//                     printf("Time %d: Process %d time quantum expired in Level %d, moving to Level %d\n", 
//                            currentTime, currentRunningProcess->processID, currentLevel + 1, newLevel + 1);
                    
//                     strcpy(currentRunningProcess->processState, "READY");
//                     if (memory[currentRunningProcess->upperMemoryBound-4].value != NULL) {
//                         free(memory[currentRunningProcess->upperMemoryBound-4].value);
//                     }
//                     memory[currentRunningProcess->upperMemoryBound-4].value = strdup("READY");
                    
//                     // Set new level and reset quantum
//                     processTable[currentProcessIndex].currentQueueLevel = newLevel;
//                     stepRemainingQuantum = mlfqScheduler.timeQuantums[newLevel];
//                     processTable[currentProcessIndex].quantumRemaining = stepRemainingQuantum;
                    
//                     // Log message about level demotion
//                     char message[150];
//                     snprintf(message, sizeof(message), 
//                              "Process %d quantum expired in Level %d, moving to Level %d", 
//                              currentRunningProcess->processID, currentLevel + 1, newLevel + 1);
//                     log_message(gui, message);
                    
//                     // Add to new queue level
//                     enqueue(&mlfqScheduler.queues[newLevel], currentRunningProcess);
//                 }
                
//                 // Clear the current process
//                 currentRunningProcess = NULL;
//             }
//         }
//     }
// }
void run_mlfq_step() {
    // Check if any new processes have arrived at the current time
    check_for_process_arrivals(currentTime);
    
    // If there's no current running process, find one from the highest priority non-empty queue
    if (currentRunningProcess == NULL) {
        for (int level = 0; level < NUM_MLFQ_LEVELS; level++) {
            currentRunningProcess = dequeue(&mlfqScheduler.queues[level]);
            if (currentRunningProcess != NULL) {
                // Found a process to run
                strcpy(currentRunningProcess->processState, "RUNNING");
                
                if (memory[currentRunningProcess->upperMemoryBound-4].value != NULL) {
                    free(memory[currentRunningProcess->upperMemoryBound-4].value);
                }
                memory[currentRunningProcess->upperMemoryBound-4].value = strdup("RUNNING");
                
                // Find the process in the process table
                currentProcessIndex = -1;
                for (int i = 0; i < numProcesses; i++) {
                    if (processTable[i].pcb != NULL && 
                        processTable[i].pcb->processID == currentRunningProcess->processID) {
                        currentProcessIndex = i;
                        
                        // Set the queue level for the process
                        processTable[i].currentQueueLevel = level;
                        
                        // Set the quantum for the process based on the queue level
                        stepRemainingQuantum = mlfqScheduler.timeQuantums[level];
                        processTable[i].quantumRemaining = stepRemainingQuantum;
                        break;
                    }
                }
                
                char message[150];
                snprintf(message, sizeof(message), 
                         "Process %d from Level %d started execution (Quantum: %d)", 
                         currentRunningProcess->processID, level + 1, stepRemainingQuantum);
                log_message(gui,message);
                
                return; // Don't execute an instruction yet when we just scheduled this process
            }
        }
        
        // If we get here, no process was found to run
        return;
    }
    
    // Update waiting time for all ready processes
    for (int i = 0; i < numProcesses; i++) {
        if (processTable[i].hasArrived && !processTable[i].isComplete &&
            (currentRunningProcess == NULL || processTable[i].pcb->processID != currentRunningProcess->processID)) {
            // Only increment waiting time for ready processes, not blocked ones
            if (strcmp(processTable[i].pcb->processState, "READY") == 0) {
                processTable[i].waitingTime++;
            }
        }
    }
    
    // Execute current process for one step if we have a running process
    if (currentRunningProcess != NULL && currentProcessIndex != -1) {
        // Execute one instruction
        int lastInstructionPosition = currentRunningProcess->upperMemoryBound - 9;
        if (currentRunningProcess->programCounter <= lastInstructionPosition) {
            int memoryIndex = currentRunningProcess->programCounter;
            
            char message[256];
            snprintf(message, sizeof(message),
                    "Executing process %d, instruction: %s (Level: %d, Quantum remaining: %d)",
                    currentRunningProcess->processID, memory[memoryIndex].value,
                    processTable[currentProcessIndex].currentQueueLevel + 1, stepRemainingQuantum);
                    log_message(gui,message);
            
            execute(memory[memoryIndex].value, currentRunningProcess);
            
            // Update program counter
            currentRunningProcess->programCounter++;
            
            // Update program counter in memory
            char tempBuffer[20];
            sprintf(tempBuffer, "%d", currentRunningProcess->programCounter);
            if (memory[currentRunningProcess->upperMemoryBound-2].value != NULL) {
                free(memory[currentRunningProcess->upperMemoryBound-2].value);
            }
            memory[currentRunningProcess->upperMemoryBound-2].value = strdup(tempBuffer);
            
            processTable[currentProcessIndex].executedTime++;
            stepRemainingQuantum--;
            processTable[currentProcessIndex].quantumRemaining = stepRemainingQuantum;
            
            // Check if process was blocked after executing the instruction
            if (strcmp(currentRunningProcess->processState, "BLOCKED") == 0) {
                snprintf(message, sizeof(message),
                         "Process %d was blocked, moving out of CPU",
                         currentRunningProcess->processID);
                         log_message(gui,message);
                
                if (memory[currentRunningProcess->upperMemoryBound-4].value != NULL) {
                    free(memory[currentRunningProcess->upperMemoryBound-4].value);
                }
                memory[currentRunningProcess->upperMemoryBound-4].value = strdup("BLOCKED");
                
                // The blocked process is already in the mutex's blocked queue
                // Reset for next process selection
                currentRunningProcess = NULL;
                currentProcessIndex = -1;
            }
            // Check if process has completed
            else if (currentRunningProcess->programCounter > lastInstructionPosition) {
                snprintf(message, sizeof(message),
                         "Process %d has completed execution",
                         currentRunningProcess->processID);
                         log_message(gui,message);
                
                strcpy(currentRunningProcess->processState, "TERMINATED");
                if (memory[currentRunningProcess->upperMemoryBound-4].value != NULL) {
                    free(memory[currentRunningProcess->upperMemoryBound-4].value);
                }
                memory[currentRunningProcess->upperMemoryBound-4].value = strdup("TERMINATED");
                
                processTable[currentProcessIndex].isComplete = true;
                memorydeallocate(currentRunningProcess->processID - 1);
                free(currentRunningProcess);
                currentRunningProcess = NULL;
                currentProcessIndex = -1;
            }
            // Check if time quantum has expired
            else if (stepRemainingQuantum <= 0) {
                int currentLevel = processTable[currentProcessIndex].currentQueueLevel;
                
                // For level 4 (lowest), use Round Robin (keep in same queue)
                if (currentLevel == NUM_MLFQ_LEVELS - 1) {
                    snprintf(message, sizeof(message),
                             "Process %d time quantum expired in Level %d (RR), returning to same queue",
                             currentRunningProcess->processID, currentLevel + 1);
                             log_message(gui,message);
                    
                    strcpy(currentRunningProcess->processState, "READY");
                    if (memory[currentRunningProcess->upperMemoryBound-4].value != NULL) {
                        free(memory[currentRunningProcess->upperMemoryBound-4].value);
                    }
                    memory[currentRunningProcess->upperMemoryBound-4].value = strdup("READY");
                    
                    // Reset quantum for RR at this level
                    stepRemainingQuantum = mlfqScheduler.timeQuantums[currentLevel];
                    processTable[currentProcessIndex].quantumRemaining = stepRemainingQuantum;
                    
                    // Add back to same queue
                    enqueue(&mlfqScheduler.queues[currentLevel], currentRunningProcess);
                } 
                // For levels 1-3, move to lower level when quantum expires
                else {
                    int newLevel = currentLevel + 1;
                    snprintf(message, sizeof(message),
                             "Process %d time quantum expired in Level %d, moving to Level %d",
                             currentRunningProcess->processID, currentLevel + 1, newLevel + 1);
                             log_message(gui,message);
                    
                    strcpy(currentRunningProcess->processState, "READY");
                    if (memory[currentRunningProcess->upperMemoryBound-4].value != NULL) {
                        free(memory[currentRunningProcess->upperMemoryBound-4].value);
                    }
                    memory[currentRunningProcess->upperMemoryBound-4].value = strdup("READY");
                    
                    // Update process table with new level and reset quantum
                    processTable[currentProcessIndex].currentQueueLevel = newLevel;
                    stepRemainingQuantum = mlfqScheduler.timeQuantums[newLevel];
                    processTable[currentProcessIndex].quantumRemaining = stepRemainingQuantum;
                    
                    // Add to the next lower priority queue
                    enqueue(&mlfqScheduler.queues[newLevel], currentRunningProcess);
                }
                
                // Reset for next process selection
                currentRunningProcess = NULL;
                currentProcessIndex = -1;
            }
        } else {
            // Process completed all instructions but somehow we're still here
            char message[150];
            snprintf(message, sizeof(message),
                     "Process %d has completed execution",
                     currentRunningProcess->processID);
                     log_message(gui,message);
            
            strcpy(currentRunningProcess->processState, "TERMINATED");
            if (memory[currentRunningProcess->upperMemoryBound-4].value != NULL) {
                free(memory[currentRunningProcess->upperMemoryBound-4].value);
            }
            memory[currentRunningProcess->upperMemoryBound-4].value = strdup("TERMINATED");
            
            processTable[currentProcessIndex].isComplete = true;
            memorydeallocate(currentRunningProcess->processID - 1);
            free(currentRunningProcess);
            currentRunningProcess = NULL;
            currentProcessIndex = -1;
        }
    } else if (currentRunningProcess != NULL) {
        // Process not found in the table
        char message[150];
        snprintf(message, sizeof(message),
                 "Error: Current process %d not found in process table",
                 currentRunningProcess->processID);
                 log_message(gui,message);
        
        currentRunningProcess = NULL;
        currentProcessIndex = -1;
    }
}
// Update execute_step function to use the step-by-step functions
// int execute_step() {
//     // Check if all processes are complete
//     if (all_processes_complete()) {
//         return 0;  // Simulation complete
//     }
// 	First increment time (from -1 to 0 on first step)
 //  	currentTime++;
//     printf("in execute_step\n");
//     // Run the appropriate scheduler step based on the selected algorithm
//     switch (algorithm) {
//         case FCFS:
//             run_fcfs_step();
//             break;
//         case ROUND_ROBIN:
//             run_rr_step();
//             break;
//         case MLFQ:
//             run_mlfq_step();
//             break;
//     }
    
//     // Increment simulation time
//     currentTime++;
    
//     return 1;  // More steps needed
// }


int execute_step() {
    // Check for process arrivals at current time first
    check_for_process_arrivals(currentTime);

    // Check if all processes are complete
    if (all_processes_complete()) {
        return 0;  // Simulation complete
    }
   
    // Run the appropriate scheduler step based on the selected algorithm
    switch (algorithm) {
        case FCFS:
            run_fcfs_step();
            break;
        case ROUND_ROBIN:
            run_rr_step();
            break;
        case MLFQ:
            run_mlfq_step();
            break;
    }
    
    // Update GUI to show the state at current time
    if (gui) {
        update_gui(gui);
    }
    
    // Increment simulation time
    
    currentTime++;
    return 1;  // More steps needed
}
    

// Reset the scheduler state
void reset_scheduler() {
    // Reset simulation time
    currentTime = -1;
    currentRunningProcess = NULL;
    currentProcessIndex = -1;
    stepRemainingQuantum = 0;
    waiting_for_input = false;
    pending_input_var[0] = '\0';
    pending_input_process = NULL;

    // Reset the ready queue
    readyQueue.front = NULL;
    readyQueue.rear = NULL;
    
    // Reset MLFQ queues
    for (int i = 0; i < NUM_MLFQ_LEVELS; i++) {
        mlfqScheduler.queues[i].front = NULL;
        mlfqScheduler.queues[i].rear = NULL;
    }

    // Reset mutex states
    fileMutex.blockedQueue = NULL;
    fileMutex.owner = NULL;
    inputMutex.blockedQueue = NULL;
    inputMutex.owner = NULL;
    outputMutex.blockedQueue = NULL;
    outputMutex.owner = NULL;
    
    // Free process table
    if (processTable != NULL) {
        for (int i = 0; i < numProcesses; i++) {
            if (processTable[i].pcb != NULL) {
                free(processTable[i].pcb);
                processTable[i].pcb = NULL;
            }
        }
        free(processTable);
        processTable = NULL;
    }
    for (int i = 0; i < memorySize; i++) {
        if (memory[i].name != NULL) {
            free(memory[i].name);
            memory[i].name = NULL;
        }
        if (memory[i].value != NULL) {
            free(memory[i].value);
            memory[i].value = NULL;
        }
        memory[i].processID = -1;
    }
    
    numProcesses = 0;

    // Reset variables
    var_count = 0;
    
    // Re-initialize the simulation
    initialize_simulation();
    
    
    log_message(gui, "Scheduler reset complete\n");
}

// void execute_assign(PCB* process, const char* var_name) {
//     handle_input_instruction(process, var_name, false);
// }

// void execute_printFromTo(PCB* process, const char* start_var, const char* end_var) {
//     if (!waiting_for_input) {
//         handle_input_instruction(process, start_var, true);
//     }
// }


// Pause the simulation
void pause_simulation() {
    if (simulationRunning) {
        simulationPaused = true;
    }
}

// Resume the simulation
void resume_simulation() {
    if (simulationRunning) {
        simulationPaused = false;
    }
}

// Stop the simulation
void stop_simulation() {
    simulationRunning = false;
}

// Reset the simulation
void reset_simulation() {
    // Clean up resources
    if (processTable != NULL) {
        for (int i = 0; i < numProcesses; i++) {
            if (processTable[i].pcb != NULL) {
                free(processTable[i].pcb);
                processTable[i].pcb = NULL;
            }
        }
        free(processTable);
        processTable = NULL;
    }
    
    // Reset memory
    for (int i = 0; i < memorySize; i++) {
        if (memory[i].name != NULL) {
            free(memory[i].name);
            memory[i].name = NULL;
        }
        if (memory[i].value != NULL) {
            free(memory[i].value);
            memory[i].value = NULL;
        }
        memory[i].processID = -1;
    }
    
    // Reset variables
    numProcesses = 0;
    currentTime = -1;
    processCount = 0;
    var_count = 0;
    simulationRunning = false;
    simulationPaused = false;
    currentRunningProcess = NULL;
    currentProcessIndex = -1;
    stepRemainingQuantum = 0;
}

// Clean up resources
void cleanupResources() {
    // Free memory allocated for processes
    if (processTable != NULL) {
        for (int i = 0; i < numProcesses; i++) {
            if (processTable[i].pcb != NULL) {
                free(processTable[i].pcb);
            }
        }
        free(processTable);
    }
    
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
    log_message(gui, "Resources cleaned up\n");
}



// Get scheduler statistics as a string
char* get_scheduler_stats_string() {
    static char stats[256];
    
    int total = 0, ready = 0, running = 0, blocked = 0, completed = 0;
    
    for (int i = 0; i < numProcesses; i++) {
        total++;
        if (processTable[i].isComplete) {
            completed++;
        } else if (processTable[i].pcb != NULL) {
            if (strcmp(processTable[i].pcb->processState, "READY") == 0) {
                ready++;
            } else if (strcmp(processTable[i].pcb->processState, "RUNNING") == 0) {
                running++;
            } else if (strcmp(processTable[i].pcb->processState, "BLOCKED") == 0) {
                blocked++;
            }
        }
    }
    
    const char* algoName;
    switch (algorithm) {
        case FCFS: algoName = "First Come First Serve"; break;
        case ROUND_ROBIN: algoName = "Round Robin"; break;
        case MLFQ: algoName = "Multi-Level Feedback Queue"; break;
        default: algoName = "Unknown";
    }
    
    sprintf(stats, "Time: %d\nAlgorithm: %s\nProcesses: %d (Ready: %d, Running: %d, Blocked: %d, Completed: %d)",
            currentTime, algoName, total, ready, running, blocked, completed);
            
    return stats;
}

// Get process state as a string
char* get_process_state_string(PCB* process) {
    if (process == NULL) {
        return "NULL";
    }
    return process->processState;
}

// Get mutex status as a string
char* get_mutex_status_string(Mutex* mutex) {
    static char status[100];
    
    if (mutex->locked) {
        sprintf(status, "%s: LOCKED (Owner: P%d)", 
                mutex->resource, 
                mutex->owner ? mutex->owner->processID : -1);
    } else {
        sprintf(status, "%s: FREE", mutex->resource);
    }
    
    return status;
}

// Get current instruction for a process
char* get_current_instruction(PCB* process) {
    static char instruction[MAX_LINE_LENGTH];
    
    if (process == NULL) {
        return "No process running";
    }
    
    if (process->programCounter <= process->upperMemoryBound - 9) {
        sprintf(instruction, "%s", memory[process->programCounter].value);
        return instruction;
    }
    
    return "End of program";
}




// Set the scheduling algorithm
void set_scheduler_algorithm(int algorithm_index) {
    switch (algorithm_index) {
        case 0:  // FCFS
            algorithm = FCFS;
            break;
        case 1:  // Round Robin
            algorithm = ROUND_ROBIN;
            break;
        case 2:  // MLFQ
            algorithm = MLFQ;
            break;
        default:
            printf("Invalid algorithm index: %d, defaulting to FCFS\n", algorithm_index);
            log_message(gui, "Invalid algorithm index: " );
            log_message(gui, "%d, defaulting to FCFS\n");
            algorithm = FCFS;
    }
    if (currentRunningProcess != NULL) {
        // Only reset if simulation has started or was running
        reset_scheduler();
    }
    char str[20];
    if(algorithm == FCFS) {
        strcpy(str, "FCFS");
    } else if (algorithm == ROUND_ROBIN) {
        strcpy(str, "Round Robin");
    } else if (algorithm == MLFQ) {
        strcpy(str, "MLFQ");
    } else {
        strcpy(str, "Unknown");
    }
    printf("Scheduler algorithm set to: %d\n", algorithm);
    log_message(gui, "Scheduler algorithm set to: ");
    log_message(gui, str);
    log_message(gui, "\n");
}

// Set the time quantum for Round Robin and MLFQ
void set_scheduler_quantum(int quantum) {
    RR_QUANTUM = quantum;
    printf("Time quantum set to %d\n", quantum);
    log_message(gui, "Time quantum set to ");
    int number = quantum;
    char str[20];
    sprintf(str, "%d", number);
    log_message(gui, str);
    log_message(gui, "\n");
    
    // Update MLFQ last level quantum if using MLFQ
    if (algorithm == MLFQ) {
        mlfqScheduler.timeQuantums[NUM_MLFQ_LEVELS-1] = quantum;
    }
}


int add_process(const char *filepath, int arrival_time) {
    initialMemory();
    if (!filepath) {
        fprintf(stderr, "Invalid file path\n");
        log_message(gui, "Invalid file path\n");
        return -1;
    }

    if (numProcesses >= MAX_PROCESSES) {
        fprintf(stderr, "Process table full\n");
        log_message(gui, "Process table full\n");
        return 0;
    }

    // Allocate process table if needed
    if (processTable == NULL) {
        processTable = (ProcessTableEntry*)malloc(MAX_PROCESSES * sizeof(ProcessTableEntry));
        if (!processTable) {
            fprintf(stderr, "Failed to allocate process table\n");
            log_message(gui, "Failed to allocate process table\n");
            return -1;
        }
    }

    int index = numProcesses;

    // Initialize process table entry
    processTable[index].id = index + 1;
    strncpy(processTable[index].filename, filepath, MAX_FILENAME_LENGTH - 1);
    processTable[index].filename[MAX_FILENAME_LENGTH - 1] = '\0';
    processTable[index].arrivalTime = arrival_time;
    processTable[index].executedTime = 0;
    processTable[index].pcb = NULL;
    processTable[index].hasArrived = false;
    processTable[index].isComplete = false;
    processTable[index].currentQueueLevel = 0;
    processTable[index].quantumRemaining = 0;
    processTable[index].waitingTime = 0;

    numProcesses++;

    printf("Added process from file %s with arrival time %d \n",
           filepath, arrival_time);
   

    return processTable[index].id;
}
// Add this function


void run_full_simulation() {
    printf("Starting full simulation with algorithm %d\n", algorithm);
    
// Initialize time to -1 (will be incremented to 0 on first step)
    currentTime = -1;
    // Run until all processes complete
    while (!all_processes_complete()) {
	    currentTime++;
        // Check for process arrivals
        check_for_process_arrivals(currentTime);
        
        // Process a step based on the algorithm
        switch (algorithm) {
            case FCFS:
                run_fcfs_step();
                break;
            case ROUND_ROBIN:
                run_rr_step();
                break;
            case MLFQ:
                run_mlfq_step();
                break;
        }
        
        // Check if we're waiting for input
        if (waiting_for_input) {
            // We need to pause the simulation and wait for input
            printf("Waiting for input...\n");
            
            // Stop the loop and let the input dialog handle resumption
            break;
        }
        
        
        // Sleep to simulate time passing - just like in original functions
        sleep(1);
    }
    
    printf("Full simulation complete at time %d\n", currentTime);
}


