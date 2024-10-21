#ifndef SCHEDULER_FCFS_H
#define SCHEDULER_FCFS_H

// Function prototypes for FCFS scheduling
void initialize_single_queue();
void add_burst_to_single_queue(burst_t burst);
burst_t pick_next_fcfs_burst(); // For FCFS scheduling
void print_final_report();      // Placeholder for Phase 1, detailed in Phase 2

#endif
