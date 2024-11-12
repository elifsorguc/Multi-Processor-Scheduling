#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <math.h>

// Constants for queue scheduling methods in multi-queue
#define RM 0 // Round-Robin Method
#define LM 1 // Load-Balancing Method

// Global variables for processors and queue type
int num_processors;
int is_multi_queue;

// Define scheduling algorithms
typedef enum
{
    FCFS, // First-Come, First-Served
    SJF   // Shortest Job First (non-preemptive)
} scheduling_algorithm;

// Structure to represent a burst (process) to be handled by a processor
typedef struct burst
{
    int pid;              // Process ID
    int cpu_id;           // CPU ID (processor) that handled this burst
    int length;           // Burst length in ms (execution time)
    long arrival_time;    // Time when burst arrives in the system
    long finish_time;     // Time when burst completes execution
    long turnaround_time; // Turnaround time = finish_time - arrival_time
    long waiting_time;    // Waiting time = turnaround_time - length
    struct burst *next;   // Pointer to next burst (for linked list in queue)
} burst_t;

// Queue structures to manage bursts
typedef struct node
{
    burst_t *burst;    // Burst stored in this node
    struct node *next; // Pointer to the next node in the queue
} node_t;

typedef struct queue
{
    node_t *head; // Head (front) of the queue
    node_t *tail; // Tail (end) of the queue
} queue_t;

// Structure for storing parsed command-line arguments
typedef struct arguments
{
    int num_processors;                   // Number of processors
    int multi_queue;                      // 1 if multi-queue, 0 if single queue
    scheduling_algorithm scheduling_algo; // Selected scheduling algorithm
    char *input_file;                     // Input file name, if provided
    int random_generation;                // 1 if generating random bursts
    int T, T1, T2, L, L1, L2, PC;         // Parameters for random generation
    int outmode;                          // Output mode (1, 2, or 3)
} arguments;

// Global variables for tracking time and controlling the simulation
long simulation_start_time;          // Start time of the simulation (in ms)
int outmode;                         // Output mode specified in arguments
volatile int simulation_running = 1; // Flag to control simulation loop

// Variables for processor threads
pthread_t *processor_threads;

// Variables for single and multi-queue scheduling
queue_t *ready_queue;                  // Single ready queue (for single-queue scheduling)
queue_t **processor_queues;            // Array of queues for multi-queue scheduling
int *processor_loads;                  // Array to track load for each processor's queue
pthread_mutex_t *queue_locks;          // Array of locks (one per queue for multi-queue)
pthread_cond_t *queue_not_empty;       // Array of condition variables (one per queue for multi-queue)
pthread_mutex_t queue_lock;            // Single lock for the single-queue approach
pthread_cond_t single_queue_not_empty; // Condition variable for single-queue approach

/*
 * init_simulation_time:
 * Initializes the simulation start time by recording the current time in milliseconds.
 */
void init_simulation_time()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    simulation_start_time = tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

/*
 * get_current_time:
 * Returns the elapsed time in milliseconds since the simulation started.
 */
long get_current_time()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000 + tv.tv_usec / 1000) - simulation_start_time;
}

/*
 * Queue operations:
 * - create_queue: Allocates and initializes a new empty queue.
 * - enqueue: Adds a burst to the end of the queue.
 * - dequeue: Removes and returns a burst from the front of the queue.
 * - is_queue_empty: Returns 1 if the queue is empty, 0 otherwise.
 * - free_queue: Frees all nodes in the queue.
 */
queue_t *create_queue()
{
    queue_t *queue = malloc(sizeof(queue_t));
    queue->head = queue->tail = NULL;
    return queue;
}

void enqueue(queue_t *queue, burst_t *burst)
{
    node_t *new_node = malloc(sizeof(node_t));
    new_node->burst = burst;
    new_node->next = NULL;
    if (queue->tail)
    {
        queue->tail->next = new_node;
    }
    else
    {
        queue->head = new_node;
    }
    queue->tail = new_node;
}

burst_t *dequeue(queue_t *queue)
{
    if (queue->head == NULL)
        return NULL;
    node_t *old_head = queue->head;
    burst_t *burst = old_head->burst;
    queue->head = old_head->next;
    if (queue->head == NULL)
    {
        queue->tail = NULL;
    }
    free(old_head);
    return burst;
}

int is_queue_empty(queue_t *queue)
{
    return queue->head == NULL;
}

void free_queue(queue_t *queue)
{
    node_t *current = queue->head;
    while (current)
    {
        node_t *next = current->next;
        free(current);
        current = next;
    }
    free(queue);
}

/*
 * parse_arguments:
 * Parses command-line arguments and populates the arguments structure.
 * Returns 1 on success, 0 on failure.
 */
int parse_arguments(int argc, char *argv[], arguments *args)
{
    if (argc < 5)
    {
        fprintf(stderr, "Insufficient arguments.\n");
        return 0;
    }
    args->num_processors = atoi(argv[2]);
    args->multi_queue = (strcmp(argv[4], "M") == 0);                    // Multi-queue if 'M'
    args->scheduling_algo = (strcmp(argv[6], "SJF") == 0) ? SJF : FCFS; // SJF or FCFS
    args->outmode = 1;                                                  // Default OUTMODE
    for (int i = 7; i < argc; i++)
    {
        if (strcmp(argv[i], "-m") == 0)
        {
            args->outmode = atoi(argv[++i]);
        }
        if (strcmp(argv[i], "-i") == 0)
        {
            args->input_file = argv[++i];
        }
        if (strcmp(argv[i], "-r") == 0)
        {
            args->random_generation = 1;
            args->T = atoi(argv[++i]);
            args->T1 = atoi(argv[++i]);
            args->T2 = atoi(argv[++i]);
            args->L = atoi(argv[++i]);
            args->L1 = atoi(argv[++i]);
            args->L2 = atoi(argv[++i]);
            args->PC = atoi(argv[++i]);
        }
    }
    return 1;
}

/*
 * initialize_multi_queues:
 * Allocates and initializes queues, locks, and condition variables for each processor
 * in multi-queue scheduling mode.
 */
void initialize_multi_queues()
{
    processor_queues = malloc(num_processors * sizeof(queue_t *));
    processor_loads = malloc(num_processors * sizeof(int));
    queue_locks = malloc(num_processors * sizeof(pthread_mutex_t));
    queue_not_empty = malloc(num_processors * sizeof(pthread_cond_t));
    for (int i = 0; i < num_processors; i++)
    {
        processor_queues[i] = create_queue();
        processor_loads[i] = 0;
        pthread_mutex_init(&queue_locks[i], NULL);
        pthread_cond_init(&queue_not_empty[i], NULL);
    }
}

/*
 * add_burst_to_queue:
 * Adds a burst to a processor's queue based on the selected queue management method.
 * - Round-Robin: Adds to processors in a round-robin sequence.
 * - Load-Balancing: Adds to the processor with the least current load.
 */
void add_burst_to_queue(burst_t *burst, int method)
{
    static int rr_index = 0;
    int target_queue = (method == RM) ? rr_index++ % num_processors : 0;
    if (method == LM)
    { // Find the least-loaded queue
        for (int i = 1; i < num_processors; i++)
        {
            if (processor_loads[i] < processor_loads[target_queue])
            {
                target_queue = i;
            }
        }
    }
    // Lock queue, add burst, update load, signal waiting threads
    pthread_mutex_lock(&queue_locks[target_queue]);
    enqueue(processor_queues[target_queue], burst);
    processor_loads[target_queue] += burst->length;
    pthread_cond_signal(&queue_not_empty[target_queue]);
    pthread_mutex_unlock(&queue_locks[target_queue]);
}

/*
 * processor_thread_function:
 * Simulates burst execution by each processor. Each processor picks a burst from its queue,
 * executes it by sleeping, records the timing metrics, and continues until all bursts are processed.
 */
void *processor_thread_function(void *arg)
{
    int processor_id = *((int *)arg); // Get processor ID
    free(arg);                        // Free dynamically allocated processor_id
    scheduling_algorithm algo = FCFS;

    while (simulation_running)
    {
        burst_t *burst = NULL;

        // Pick burst from the appropriate queue
        if (is_multi_queue)
        {
            pthread_mutex_lock(&queue_locks[processor_id]);
            while (is_queue_empty(processor_queues[processor_id]) && simulation_running)
            {
                pthread_cond_wait(&queue_not_empty[processor_id], &queue_locks[processor_id]);
            }
            if (!simulation_running)
            {
                pthread_mutex_unlock(&queue_locks[processor_id]);
                break;
            }
            burst = dequeue(processor_queues[processor_id]);
            processor_loads[processor_id] -= burst->length;
            pthread_mutex_unlock(&queue_locks[processor_id]);
        }
        else
        {
            pthread_mutex_lock(&queue_lock);
            while (is_queue_empty(ready_queue) && simulation_running)
            {
                pthread_cond_wait(&single_queue_not_empty, &queue_lock);
            }
            burst = dequeue(ready_queue);
            pthread_mutex_unlock(&queue_lock);
        }

        // Simulate burst processing (sleep), record timing information
        if (burst)
        {
            burst->finish_time = get_current_time();
            usleep(burst->length * 1000);
            burst->turnaround_time = burst->finish_time - burst->arrival_time;
            burst->waiting_time = burst->turnaround_time - burst->length;
        }
    }
    return NULL;
}

/*
 * create_processor_threads:
 * Creates a separate thread for each processor to process bursts from its queue.
 */
void create_processor_threads()
{
    processor_threads = malloc(num_processors * sizeof(pthread_t));
    for (int i = 0; i < num_processors; i++)
    {
        int *id = malloc(sizeof(int));
        *id = i;
        pthread_create(&processor_threads[i], NULL, processor_thread_function, id);
    }
}

/*
 * wait_for_threads_to_finish:
 * Waits for all processor threads to complete processing.
 */
void wait_for_threads_to_finish()
{
    for (int i = 0; i < num_processors; i++)
    {
        pthread_join(processor_threads[i], NULL);
    }
}

/*
 * cleanup:
 * Cleans up resources by freeing memory and destroying mutexes and condition variables.
 */
void cleanup()
{
    free(processor_threads);
    for (int i = 0; i < num_processors; i++)
    {
        free_queue(processor_queues[i]);
        pthread_mutex_destroy(&queue_locks[i]);
        pthread_cond_destroy(&queue_not_empty[i]);
    }
    free(processor_queues);
    free(processor_loads);
    free(queue_locks);
    free(queue_not_empty);
}

/*
 * main:
 * The main function of the program. Parses command-line arguments, initializes resources,
 * starts processor threads, and manages bursts based on the input file or random generation.
 */
int main(int argc, char *argv[])
{
    arguments args;
    if (!parse_arguments(argc, argv, &args))
        return EXIT_FAILURE;

    num_processors = args.num_processors;
    is_multi_queue = args.multi_queue;
    outmode = args.outmode;
    init_simulation_time();

    // Initialize single or multi-queue setup based on arguments
    if (is_multi_queue)
    {
        initialize_multi_queues();
    }
    else
    {
        ready_queue = create_queue();
        pthread_mutex_init(&queue_lock, NULL);
        pthread_cond_init(&single_queue_not_empty, NULL);
    }

    // Start processor threads
    create_processor_threads();

    // Add workload reading and generation logic here...
    // Note: This requires adding file reading or random generation for bursts.

    // Wait for all threads to complete processing
    wait_for_threads_to_finish();

    // Clean up allocated resources
    cleanup();

    return EXIT_SUCCESS;
}
