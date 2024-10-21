#ifndef SCHEDULER_MULTIQUEUE_H
#define SCHEDULER_MULTIQUEUE_H

#include "scheduler_fcfs.h"

// Function prototypes for multi-queue scheduling
void initialize_multi_queues(int num_processors);
void add_burst_to_multi_queue(burst_t burst, int method); // Method: RM or LM
burst_t pick_next_burst_multi_queue(int processor_id, scheduling_algorithm algo);
void print_final_report_multi(); // Detailed final report for multi-queue

#endif
