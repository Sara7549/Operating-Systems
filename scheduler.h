#ifndef SCHEDULER_H
#define SCHEDULER_H
#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include "GUI.h"




// Define constants
#define MAX_PROCESSES 10
#define MAX_PRIORITY 3  
#define MAX_VARIABLES 100
#define MAX_LINE_LENGTH 256
#define MAX_FILENAME_LENGTH 50
#define MAX_CONTENT_LENGTH 1024
#define MAX_INSTRUCTIONS 20
#define INSTRUCTION_SIZE 50
#define STATE_SIZE 20
#define NUM_MLFQ_LEVELS 4  // 4 levels for MLFQ


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
    char filename[MAX_FILENAME_LENGTH];
    int id; // Process ID
    PCB* pcb;
    int arrivalTime;
    int burstTime;
    int priority; 
    int executedTime;
    bool hasArrived;
    bool isComplete;
    int currentQueueLevel;  // For MLFQ: 0-3 (0 is highest priority)
    int quantumRemaining;
    int waitingTime;   // For MLFQ: track quantum within current level
} ProcessTableEntry;

// Multi-Level Feedback Queue
typedef struct {
    Queue queues[NUM_MLFQ_LEVELS]; // 4 queue levels
    int timeQuantums[NUM_MLFQ_LEVELS]; // Time quantum for each level
} MLFQScheduler;

typedef struct {
    char resource[20];
    int locked; // is it available or not (0=available, 1= locked)
    PCB* owner; //points to the process that is using the resource to make sure it unlocks it later (and not another process)
    PCB* blockedQueue; // queue that contains the blocked processes
} Mutex;
// Forward declarations for types


// Scheduling algorithm enum
typedef enum {
    FCFS = 0,
    ROUND_ROBIN = 1,
    MLFQ = 2
} SchedulingAlgorithm;

extern memoryWord *memory;
extern int memorySize;
extern ProcessTableEntry* processTable;
extern int numProcesses;
extern Queue readyQueue;
extern MLFQScheduler mlfqScheduler;
extern SchedulingAlgorithm algorithm;
extern int currentTime;
extern int processCount;
extern Variable variables[];
extern int var_count;
extern Mutex fileMutex, inputMutex, outputMutex;
extern PCB* currentRunningProcess;
extern bool simulationPaused;
extern bool simulationRunning;
extern int stepRemainingQuantum;
extern int currentProcessIndex;
extern int RR_QUANTUM;
extern bool waiting_for_input;
extern char pending_input_var[100];
extern PCB* pending_input_process;
extern PCB* currentRunningProcess;
extern int stepRemainingQuantum;
extern int currentProcessIndex;
// Additional variables for step-based execution



// Function declarations
void initialize_simulation();
int add_process(const char *filepath, int arrival_time);
void check_for_process_arrivals(int currentTime);
void initialMemory();
int execute_step();
bool all_processes_complete();
bool load_process_file(const char *filepath, int arrival_time);
void reset_scheduler();
void set_scheduler_algorithm(int algorithm_index);
void set_scheduler_quantum(int quantum);
char* get_basename(const char* path);
char* get_scheduler_stats_string();
void scheduler_log(const char* format, ...);
char* get_process_state_string(PCB* process);
void reset_simulation();
void run_fcfs_step();
void run_rr_step();
void run_mlfq_step();
void setVariable(const char* name, const char* value);
void getVariable(const char* name, char* value);
void run_full_simulation();



#endif // SCHEDULER_H
