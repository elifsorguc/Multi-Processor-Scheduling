#ifndef SCHEDULER_FCFS_H
#define SCHEDULER_FCFS_H

#include "utils.h"

// Function declarations for single-queue FCFS scheduling
void initialize_single_queue();
void add_burst_to_single_queue(burst_t *burst);
burst_t *pick_next_fcfs_burst();
void print_final_report();
void cleanup_single_queue();

#endif // SCHEDULER_FCFS_H