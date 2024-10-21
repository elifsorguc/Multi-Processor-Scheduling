#ifndef UTILS_H
#define UTILS_H

#include "scheduler_fcfs.h" // Include scheduling types and structures

// Structure to hold command-line argument information
typedef struct
{
    int num_processors;    // Number of processors (-n N)
    int multi_queue;       // 1 if multi-queue (-a M), 0 for single-queue (-a S)
    int scheduling_algo;   // FCFS or SJF (-s ALG)
    char *input_file;      // Input file name (-i INFILE)
    int random_generation; // 1 if using random burst generation (-r T T1 T2 L L1 L2 PC)
    int T, T1, T2;         // Interarrival time parameters for random generation
    int L, L1, L2;         // Burst length parameters for random generation
    int PC;                // Number of bursts to generate
} arguments;

// Function prototypes
int parse_arguments(int argc, char *argv[], arguments *args);
void read_bursts_from_file(char *filename);
void generate_random_bursts(arguments args);

#endif
