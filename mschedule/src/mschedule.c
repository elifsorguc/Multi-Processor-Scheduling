#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <math.h>
#include <time.h>

#define MAX_BURSTS 1000
FILE *outfile = NULL;
// 1 for Round Robin 0 for load balancing for queue scheduling methods in multi-queue
#define RM 0
#define LM 1

int num_processors;
int is_multi_queue;

// scheduling algorithms
typedef enum
{
    FCFS,
    SJF
} scheduling_algorithm;

scheduling_algorithm selected_algorithm;

// burst (process)
typedef struct burst
{
    int pid;
    int cpu_id;
    int length;
    long arrival_time;
    long finish_time;
    long turnaround_time; // Turnaround time = finish_time - arrival_time
    long waiting_time;    // Waiting time = turnaround_time - length
    struct burst *next;   // ptr to next burst
} burst_t;

// queue to manage bursts
typedef struct node
{
    burst_t *burst;    // burst stored in this node
    struct node *next; // ptr to the next node
} node_t;

typedef struct queue
{
    node_t *head;
    node_t *tail;
} queue_t;

// command-line arguments to parse
typedef struct arguments
{
    int num_processors;
    int multi_queue; // 1 if multi-queue, 0 if single queue
    scheduling_algorithm scheduling_algo;
    char *input_file;             // input file name (optional*)
    int random_generation;        // 1 if generating random bursts (optional*)
    int T, T1, T2, L, L1, L2, PC; // Parameters for random generation (optional*)
    int outmode;                  // Output mode (1, 2, or 3)
} arguments;

long simulation_start_time; // ms
int outmode;
volatile int simulation_running = 1; // flag to control simulation loop

pthread_t *processor_threads;

// single and multi-queue scheduling
queue_t *ready_queue = NULL;            // Single ready queue (for single-queue)
queue_t **processor_queues = NULL;      // queues for multi-queue scheduling
int *processor_loads = NULL;            // track load for each processor's queue
pthread_mutex_t *queue_locks = NULL;    // locks (one per queue for multi-queue)
pthread_cond_t *queue_not_empty = NULL; // condition variables (one per queue for multi-queue)
pthread_mutex_t queue_lock;             // single-queue lock
pthread_cond_t single_queue_not_empty;  // single-queue cv

burst_t **completed_bursts;
int burst_count = 0;

void init_simulation_time();
long get_current_time();
int parse_arguments(int argc, char *argv[], arguments *args);
int generate_random_time(int mean, int min, int max);

queue_t *create_queue();
void enqueue(queue_t *queue, burst_t *burst);
burst_t *dequeue(queue_t *queue);
int is_queue_empty(queue_t *queue);
void free_queue(queue_t *queue);
void initialize_multi_queues();
void add_burst_to_queue(burst_t *burst, int method);

void *processor_thread_function(void *arg);
void create_processor_threads();
void wait_for_threads_to_finish();

void print_summary();
void cleanup();

int compare_bursts_by_pid(const void *a, const void *b);
void add_completed_burst(burst_t *burst);
void read_bursts_from_file(const char *filename);
burst_t *dequeue_fcfs(queue_t *queue);
burst_t *dequeue_sjf(queue_t *queue);

// to store completed burst
void add_completed_burst(burst_t *burst)
{
    if (burst_count < MAX_BURSTS)
    {
        completed_bursts[burst_count++] = burst;
    }
    else
    {
        fprintf(stderr, "Error: Maximum burst count exceeded\n");
    }
}

// simulation start time by recording the current time in ms
void init_simulation_time()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    simulation_start_time = tv.tv_sec * 1000 + tv.tv_usec / 1000;
    if (outmode == 3)
        printf("Simulation start time initialized to %ld ms\n", simulation_start_time);
}

// elapsed time in ms since start
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
    if (outmode == 3)
        printf("Queue created\n");
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
    if (outmode == 3)
        fprintf(outfile, "time=%ld, Added burst to queue, pid=%d, burstlen=%d\n", get_current_time(), burst->pid, burst->length);

    // signal waiting threads that a new burst has been added
    if (is_multi_queue)
    {
        // broadcast to the cv specific to the processor's queue
        for (int i = 0; i < num_processors; i++)
        {
            pthread_cond_signal(&queue_not_empty[i]);
        }
    }
    else
    {
        // !single queue - signal the single queue's cv
        pthread_cond_signal(&single_queue_not_empty);
    }
}

void set_scheduling_algorithm(scheduling_algorithm algo)
{
    selected_algorithm = algo;
}

// dequeue function to handle both FCFS and SJF
burst_t *dequeue(queue_t *queue)
{
    if (selected_algorithm == FCFS)
    {
        return dequeue_fcfs(queue);
    }
    else if (selected_algorithm == SJF)
    {
        return dequeue_sjf(queue);
    }
    return NULL;
}

// FCFS dequeue - removes the head
burst_t *dequeue_fcfs(queue_t *queue)
{
    if (queue->head == NULL)
        return NULL;
    node_t *old_head = queue->head;
    burst_t *burst = old_head->burst;
    queue->head = old_head->next;
    if (queue->head == NULL)
        queue->tail = NULL;
    free(old_head);
    return burst;
}

// SJF dequeue - finds the shortest burst and removes it
burst_t *dequeue_sjf(queue_t *queue)
{
    if (queue->head == NULL)
    {
        return NULL;
    }

    node_t *prev = NULL;
    node_t *shortest_prev = NULL;
    node_t *current = queue->head;
    node_t *shortest_node = NULL;
    long current_time = get_current_time();

    // to find the shortest burst that already ***arrived***
    while (current != NULL)
    {
        if (current->burst->arrival_time <= current_time)
        { // check
            if (shortest_node == NULL || current->burst->length < shortest_node->burst->length)
            {
                shortest_prev = prev;
                shortest_node = current;
            }
        }
        prev = current;
        current = current->next;
    }

    // no bursts
    if (shortest_node == NULL)
    {
        return NULL;
    }

    // remove the shortest
    burst_t *burst = shortest_node->burst;
    if (shortest_node == queue->head)
    {
        queue->head = shortest_node->next;
        if (queue->head == NULL)
        {
            queue->tail = NULL;
        }
    }
    else
    {
        shortest_prev->next = shortest_node->next;
        if (shortest_node == queue->tail)
        {
            queue->tail = shortest_prev;
        }
    }
    free(shortest_node);
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
        if (current->burst != NULL)
        {
            free(current->burst);
        }
        free(current);
        current = next;
    }
    free(queue);
}

// 1 on success, 0 on failure.
int parse_arguments(int argc, char *argv[], arguments *args)
{
    if (argc < 5)
    {
        fprintf(stderr, "Insufficient arguments.\n");
        return 0;
    }

    // defaults
    args->num_processors = -1;
    args->multi_queue = 0;
    args->scheduling_algo = FCFS;
    args->outmode = 1;
    args->input_file = NULL;
    args->random_generation = 0;
    char *output_filename = NULL;

    // parse arguments in the expected order
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-n") == 0)
        {
            args->num_processors = atoi(argv[++i]);
            if (args->num_processors < 1 || args->num_processors > 64)
            {
                fprintf(stderr, "Error: -n must be between 1 and 64.\n");
                return 0;
            }
        }
        else if (strcmp(argv[i], "-a") == 0)
        {
            if (strcmp(argv[++i], "M") == 0)
            {
                args->multi_queue = 1;
                if (args->num_processors <= 1)
                {
                    fprintf(stderr, "Error: -a M requires -n to be greater than 1.\n");
                    return 0;
                }
                if (strcmp(argv[++i], "RM") == 0)
                {
                    args->multi_queue = RM;
                }
                else if (strcmp(argv[i], "LM") == 0)
                {
                    args->multi_queue = LM;
                }
                else
                {
                    fprintf(stderr, "Error: Invalid queue selection method for multi-queue. Use RM or LM.\n");
                    return 0;
                }
            }
            else if (strcmp(argv[i], "S") == 0)
            {
                args->multi_queue = 0;
                if (strcmp(argv[++i], "NA") != 0)
                {
                    fprintf(stderr, "Error: For single-queue (-a S), use NA as queue selection.\n");
                    return 0;
                }
            }
            else
            {
                fprintf(stderr, "Error: Invalid scheduling approach. Use S or M.\n");
                return 0;
            }
        }
        else if (strcmp(argv[i], "-s") == 0)
        {
            if (strcmp(argv[++i], "SJF") == 0)
            {
                args->scheduling_algo = SJF;
            }
            else if (strcmp(argv[i], "FCFS") == 0)
            {
                args->scheduling_algo = FCFS;
            }
            else
            {
                fprintf(stderr, "Error: Invalid scheduling algorithm. Use FCFS or SJF.\n");
                return 0;
            }
        }
        else if (strcmp(argv[i], "-i") == 0)
        {
            if (args->random_generation)
            {
                fprintf(stderr, "Error: Specify either -i or -r, not both.\n");
                return 0;
            }
            args->input_file = argv[++i];
        }
        else if (strcmp(argv[i], "-r") == 0)
        {
            if (args->input_file)
            {
                fprintf(stderr, "Error: Specify either -i or -r, not both.\n");
                return 0;
            }
            args->random_generation = 1;
            args->T = atoi(argv[++i]);
            args->T1 = atoi(argv[++i]);
            args->T2 = atoi(argv[++i]);
            args->L = atoi(argv[++i]);
            args->L1 = atoi(argv[++i]);
            args->L2 = atoi(argv[++i]);
            args->PC = atoi(argv[++i]);
            if (args->T <= 0 || args->T1 <= 0 || args->T2 <= 0 || args->L <= 0 || args->L1 <= 0 || args->L2 <= 0 || args->PC <= 0)
            {
                fprintf(stderr, "Error: All -r parameters must be positive integers.\n");
                return 0;
            }
            if (args->T1 > args->T2 || args->L1 > args->L2)
            {
                fprintf(stderr, "Error: Ensure T1 <= T2 and L1 <= L2.\n");
                return 0;
            }
        }
        else if (strcmp(argv[i], "-m") == 0)
        {
            args->outmode = atoi(argv[++i]);
            if (args->outmode < 1 || args->outmode > 3)
            {
                fprintf(stderr, "Error: -m must be 1, 2, or 3.\n");
                return 0;
            }
        }
        else if (strcmp(argv[i], "-o") == 0)
        {
            output_filename = argv[++i];
        }
        else
        {
            fprintf(stderr, "Error: Unknown option %s.\n", argv[i]);
            return 0;
        }
    }

    // Final checks
    if (!args->input_file && !args->random_generation)
    {
        fprintf(stderr, "Error: Specify either -i or -r.\n");
        return 0;
    }

    // Open output file if specified
    outfile = output_filename ? fopen(output_filename, "w") : stdout;
    if (output_filename && !outfile)
    {
        perror("Error opening output file");
        return 0;
    }

    printf("Parsed arguments: num_processors = %d, multi_queue = %d, scheduling_algo = %d, outmode = %d\n",
           args->num_processors, args->multi_queue, args->scheduling_algo, args->outmode);

    return 1;
}

// generate_random_time:
// Generates a random time using an exponential distribution, clamped to [min, max].
// Used to simulate interarrival times and burst lengths when generating random bursts.
int generate_random_time(int mean, int min, int max)
{
    if (min > max)
    {
        int temp = min;
        min = max;
        max = temp;
    }
    double lambda = 1.0 / mean; // rate for exponential distribution
    int random_time;
    do
    {
        double u = (double)rand() / RAND_MAX;
        random_time = (int)(-log(1.0 - u) / lambda);
    } while (random_time < min || random_time > max);
    return random_time;
}

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

// add_burst_to_queue
// Adds a burst to a processor's queue based on the selected queue management method.
// Round-Robin -> Adds to processors in a round-robin sequence.
// Load-Balancing -> Adds to the processor with the least current load.
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
    // lock queue, add burst, update load, signal waiting threads
    pthread_mutex_lock(&queue_locks[target_queue]);
    enqueue(processor_queues[target_queue], burst);
    processor_loads[target_queue] += burst->length;
    pthread_cond_signal(&queue_not_empty[target_queue]);
    pthread_mutex_unlock(&queue_locks[target_queue]);
}

// processor_thread_function
// burst execution by each processor.
// Each processor picks a burst from its queue executes it by sleeping
void *processor_thread_function(void *arg)
{
    int processor_id = *((int *)arg);
    free(arg);
    while (simulation_running)
    {
        burst_t *burst = NULL;

        // find burst from the selected queue
        if (is_multi_queue)
        {
            pthread_mutex_lock(&queue_locks[processor_id]);
            while (is_queue_empty(processor_queues[processor_id]) && simulation_running)
            {
                pthread_cond_wait(&queue_not_empty[processor_id], &queue_locks[processor_id]);
            }

            if (!simulation_running && is_queue_empty(processor_queues[processor_id]))
            {
                pthread_mutex_unlock(&queue_locks[processor_id]);
                break;
            }

            burst = dequeue(processor_queues[processor_id]);
            pthread_mutex_unlock(&queue_locks[processor_id]);
        }
        else
        {
            pthread_mutex_lock(&queue_lock);
            while (is_queue_empty(ready_queue) && simulation_running)
            {
                pthread_cond_wait(&single_queue_not_empty, &queue_lock);
            }

            if (!simulation_running && is_queue_empty(ready_queue))
            {
                pthread_mutex_unlock(&queue_lock);
                break;
            }

            burst = dequeue(ready_queue);
            pthread_mutex_unlock(&queue_lock);
        }

        if (burst)
        {
            burst->cpu_id = processor_id;

            if (outmode >= 2)
                fprintf(outfile, "time=%ld, cpu=%d, pid=%d, burstlen=%d\n", get_current_time(), processor_id, burst->pid, burst->length);

            usleep(burst->length * 1000); // simulate processing by sleeping

            // assign finish time after the burst is processed
            burst->finish_time = get_current_time();
            burst->turnaround_time = burst->finish_time - burst->arrival_time;
            burst->waiting_time = burst->turnaround_time - burst->length;

            // error checking for invalid turnaround or waiting time
            if (burst->turnaround_time < 0 || burst->waiting_time < 0)
            {
                printf("Error: Invalid turnaround or waiting time for PID %d\n", burst->pid);
            }

            if (outmode == 3)
                fprintf(outfile, "time=%ld, Burst finished, cpu=%d, pid=%d\n", get_current_time(), processor_id, burst->pid);

            // keep the completed burst for final summary****
            add_completed_burst(burst);
        }
    }
    return NULL;
}

// Creates a separate thread for each processor to process bursts from its queue.
void create_processor_threads()
{
    processor_threads = malloc(num_processors * sizeof(pthread_t));
    for (int i = 0; i < num_processors; i++)
    {
        int *id = malloc(sizeof(int));
        *id = i;
        pthread_create(&processor_threads[i], NULL, processor_thread_function, id);
        if (outmode == 3)
            printf("Created thread for processor %d\n", i);
    }
}

void wait_for_threads_to_finish()
{
    for (int i = 0; i < num_processors; i++)
    {
        pthread_join(processor_threads[i], NULL);
        if (outmode == 3)
            printf("Joined thread for processor %d\n", i);
    }
}

// freeing memory and destroying mutexes and condition variables.
void cleanup()
{
    if (processor_threads)
        free(processor_threads);
    if (ready_queue)
    {
        free_queue(ready_queue);
        pthread_mutex_destroy(&queue_lock);
        pthread_cond_destroy(&single_queue_not_empty);
    }

    if (processor_queues)
    {
        for (int i = 0; i < num_processors; i++)
        {
            if (processor_queues[i])
                free_queue(processor_queues[i]);
            pthread_mutex_destroy(&queue_locks[i]);
            pthread_cond_destroy(&queue_not_empty[i]);
        }
        free(processor_queues);
        free(processor_loads);
        free(queue_locks);
        free(queue_not_empty);
    }

    if (outfile && outfile != stdout)
    {
        fclose(outfile);
    }
}

int compare_bursts_by_pid(const void *a, const void *b)
{
    burst_t *burst_a = *(burst_t **)a;
    burst_t *burst_b = *(burst_t **)b;
    return burst_a->pid - burst_b->pid;
}

void print_summary()
{
    long total_turnaround_time = 0;

    qsort(completed_bursts, burst_count, sizeof(burst_t *), compare_bursts_by_pid);

    fprintf(outfile, "pid   cpu  burstlen   arv    finish    waitingtime   turnaround\n");

    for (int i = 0; i < burst_count; i++)
    {
        burst_t *burst = completed_bursts[i];

        fprintf(outfile, "%-4d  %-3d  %-9d  %-6ld  %-9ld  %-12ld  %-10ld\n",
                burst->pid, burst->cpu_id, burst->length, burst->arrival_time,
                burst->finish_time, burst->waiting_time, burst->turnaround_time);

        total_turnaround_time += burst->turnaround_time;
    }

    // avoid NaN by checking if burst_count > 0
    if (burst_count > 0)
    {
        double avg_turnaround_time = (double)total_turnaround_time / burst_count;
        // printf("average turnaround time: %.2f ms\n", avg_turnaround_time);
        fprintf(outfile, "average turnaround time: %.2f ms\n", avg_turnaround_time);
    }
    else
    {
        fprintf(outfile, "No bursts processed.\n");
        // printf("No bursts processed.\n");
    }
}

void read_bursts_from_file(const char *filename)
{
    FILE *file = fopen(filename, "r");
    if (!file)
    {
        perror("Error opening input file");
        exit(EXIT_FAILURE);
    }

    char line[256];
    int burst_length, inter_arrival_time, pid = 0;

    while (fgets(line, sizeof(line), file))
    {
        if (sscanf(line, "PL %d", &burst_length) == 1)
        {
            burst_t *new_burst = malloc(sizeof(burst_t));
            new_burst->pid = pid++;
            new_burst->arrival_time = get_current_time();
            new_burst->length = burst_length;
            new_burst->next = NULL;

            if (is_multi_queue)
            {
                add_burst_to_queue(new_burst, RM); // Round-Robin for multi-queue
            }
            else
            {
                pthread_mutex_lock(&queue_lock);
                enqueue(ready_queue, new_burst);
                pthread_cond_signal(&single_queue_not_empty);
                pthread_mutex_unlock(&queue_lock);
            }
        }
        else if (sscanf(line, "IAT %d", &inter_arrival_time) == 1)
        {
            // for the inter-arrival time (simulating arrival delay)
            usleep(inter_arrival_time * 1000);
        }
    }

    fclose(file);
}

int main(int argc, char *argv[])
{
    arguments args;
    if (!parse_arguments(argc, argv, &args))
        return EXIT_FAILURE;

    num_processors = args.num_processors;
    is_multi_queue = args.multi_queue;
    outmode = args.outmode;
    init_simulation_time();

    // completed_bursts array with MAX_BURSTS
    completed_bursts = malloc(MAX_BURSTS * sizeof(burst_t *));
    if (!completed_bursts)
    {
        perror("Error allocating memory for completed bursts");
        return EXIT_FAILURE;
    }

    //  selected scheduling algorithm
    set_scheduling_algorithm(args.scheduling_algo);

    // queues and threads
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
    create_processor_threads();

    // input file - generate random bursts
    if (args.input_file)
    {
        read_bursts_from_file(args.input_file);
    }
    else if (args.random_generation)
    {
        srand(time(NULL));
        for (int i = 0; i < args.PC; i++)
        {
            burst_t *new_burst = malloc(sizeof(burst_t));
            new_burst->pid = i;
            new_burst->arrival_time = get_current_time();
            new_burst->length = generate_random_time(args.T, args.L1, args.L2);
            new_burst->next = NULL;

            if (is_multi_queue)
            {
                add_burst_to_queue(new_burst, RM);
            }
            else
            {
                pthread_mutex_lock(&queue_lock);
                enqueue(ready_queue, new_burst);
                pthread_cond_signal(&single_queue_not_empty);
                pthread_mutex_unlock(&queue_lock);
            }
            usleep(generate_random_time(args.T, args.T1, args.T2) * 1000);
        }
    }

    // the end - clean up - print summary
    simulation_running = 0;
    if (is_multi_queue)
    {
        for (int i = 0; i < num_processors; i++)
        {
            pthread_cond_broadcast(&queue_not_empty[i]);
        }
    }
    else
    {
        pthread_cond_broadcast(&single_queue_not_empty);
    }
    wait_for_threads_to_finish();
    print_summary();
    cleanup();

    return EXIT_SUCCESS;
}