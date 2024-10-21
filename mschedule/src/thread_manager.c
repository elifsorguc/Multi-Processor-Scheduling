#include <pthread.h>
#include <stdio.h>
#include "scheduler_fcfs.h"
#include "scheduler_multiqueue.h"
#include "thread_manager.h"

pthread_t *processor_threads;
pthread_mutex_t queue_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queue_not_empty = PTHREAD_COND_INITIALIZER;

int num_processors; // Number of processors passed via command-line

// Processor thread function to handle both single and multi-queue scheduling
void *processor_thread_function(void *arg)
{
    int processor_id = *((int *)arg);
    scheduling_algorithm algo = FCFS; // This will be passed in real case

    while (1)
    {
        pthread_mutex_lock(&queue_lock);

        if (is_multi_queue())
        {
            while (is_queue_empty(processor_queues[processor_id]))
            {
                pthread_cond_wait(&queue_not_empty, &queue_lock);
            }

            // Pick next burst for this processor
            burst_t burst = pick_next_burst_multi_queue(processor_id, algo);
            pthread_mutex_unlock(&queue_lock);

            if (burst)
            {
                usleep(burst.length * 1000); // Simulate running the burst
                // Update burst's metrics (finish time, etc.)
            }
        }
        else
        {
            while (is_queue_empty(ready_queue))
            {
                pthread_cond_wait(&queue_not_empty, &queue_lock);
            }

            // Pick the next burst from the single FCFS queue
            burst_t burst = pick_next_fcfs_burst();
            pthread_mutex_unlock(&queue_lock);

            if (burst)
            {
                usleep(burst.length * 1000); // Simulate running the burst
                // Update burst's metrics (finish time, etc.)
            }
        }
    }
}

// Create threads for processors
void create_processor_threads(int num_processors_arg, scheduling_algorithm algo)
{
    num_processors = num_processors_arg; // Store the number of processors
    processor_threads = malloc(num_processors * sizeof(pthread_t));

    for (int i = 0; i < num_processors; i++)
    {
        pthread_create(&processor_threads[i], NULL, processor_thread_function, (void *)&i);
    }
}

// Wait for all threads to finish
void wait_for_threads_to_finish()
{
    for (int i = 0; i < num_processors; i++)
    {
        pthread_join(processor_threads[i], NULL);
    }
}
