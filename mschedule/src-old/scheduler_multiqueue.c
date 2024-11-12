#include <stdio.h>
#include <stdlib.h>
#include "scheduler_multiqueue.h"
#include "thread_manager.h"

queue_t **processor_queues; // Array of queues, one per processor
int *processor_loads;       // Array to track the total load of each processor

// Initialize multiple queues (one per processor)
void initialize_multi_queues(int num_processors)
{
    processor_queues = malloc(num_processors * sizeof(queue_t *));
    processor_loads = malloc(num_processors * sizeof(int));

    for (int i = 0; i < num_processors; i++)
    {
        processor_queues[i] = create_queue(); // Create a queue for each processor
        processor_loads[i] = 0;               // Initialize load to 0
    }
}

// Add a burst to the appropriate queue based on method (Round-Robin or Load-Balancing)
void add_burst_to_multi_queue(burst_t burst, int method)
{
    static int round_robin_index = 0;

    if (method == RM)
    { // Round-Robin method
        int target_queue = round_robin_index % num_processors;
        enqueue(processor_queues[target_queue], burst);
        processor_loads[target_queue] += burst.length;
        round_robin_index++;
    }
    else if (method == LM)
    { // Load-Balancing method
        int min_load_index = 0;

        // Find the queue with the least load
        for (int i = 1; i < num_processors; i++)
        {
            if (processor_loads[i] < processor_loads[min_load_index])
            {
                min_load_index = i;
            }
        }

        enqueue(processor_queues[min_load_index], burst);
        processor_loads[min_load_index] += burst.length;
    }
}

// Pick the next burst from the multi-queue, based on the scheduling algorithm
burst_t pick_next_burst_multi_queue(int processor_id, scheduling_algorithm algo)
{
    queue_t *processor_queue = processor_queues[processor_id];

    if (algo == FCFS)
    {
        return dequeue(processor_queue); // Pick the first burst (FCFS)
    }
    else if (algo == SJF)
    {
        return pick_shortest_burst(processor_queue); // Pick the shortest burst (SJF)
    }

    return NULL; // If no valid burst found, return NULL
}

// Pick the shortest burst from the queue (used for SJF)
burst_t pick_shortest_burst(queue_t *queue)
{
    burst_t shortest_burst = NULL;
    node_t *current = queue->head;
    node_t *shortest_node = NULL;

    while (current != NULL)
    {
        burst_t burst = current->burst;
        if (shortest_burst == NULL || burst.length < shortest_burst.length)
        {
            shortest_burst = burst;
            shortest_node = current;
        }
        current = current->next;
    }

    if (shortest_node != NULL)
    {
        remove_node(queue, shortest_node); // Remove the shortest burst from the queue
    }

    return shortest_burst;
}

// Final report for multi-queue scheduling
void print_final_report_multi()
{
    // Print the same detailed output as in FCFS report
    printf("Detailed report for multi-queue scheduling...\n");
    // This should print out each burst's final metrics: pid, cpu-id, arrival, finish, waiting, etc.
    // And calculate average turnaround time
}
