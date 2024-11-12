#include <stdio.h>
#include "scheduler_fcfs.h"
#include "thread_manager.h"

queue_t *ready_queue; // Single ready queue for FCFS scheduling

void initialize_single_queue()
{
    ready_queue = create_queue();
}

// Add a burst to the FCFS queue
void add_burst_to_single_queue(burst_t burst)
{
    enqueue(ready_queue, burst);
}

// Pick the next burst using FCFS from the queue
burst_t pick_next_fcfs_burst()
{
    return dequeue(ready_queue); // Dequeue burst from the head of the queue
}

// Placeholder for final report, detailed in Phase 2
void print_final_report()
{
    printf("Final report: Placeholder for Phase 2.\n");
}
