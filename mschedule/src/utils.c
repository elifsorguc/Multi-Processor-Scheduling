#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utils.h"

// Function to parse command-line arguments and fill the 'arguments' struct
int parse_arguments(int argc, char *argv[], arguments *args)
{
    if (argc < 5)
    {
        printf("Error: Not enough arguments provided.\n");
        return 0; // Error, insufficient arguments
    }

    // Initialize default values
    args->num_processors = 1;
    args->multi_queue = 0;        // Default is single-queue
    args->scheduling_algo = FCFS; // Default to FCFS
    args->input_file = NULL;
    args->random_generation = 0;

    // Parse -n N (number of processors)
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-n") == 0 && i + 1 < argc)
        {
            args->num_processors = atoi(argv[++i]);
        }
        // Parse -a S or M (single or multi-queue)
        else if (strcmp(argv[i], "-a") == 0 && i + 1 < argc)
        {
            if (strcmp(argv[++i], "M") == 0)
            {
                args->multi_queue = 1; // Multi-queue
            }
            else if (strcmp(argv[i], "S") == 0)
            {
                args->multi_queue = 0; // Single-queue
            }
        }
        // Parse -s FCFS or SJF (scheduling algorithm)
        else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc)
        {
            if (strcmp(argv[++i], "FCFS") == 0)
            {
                args->scheduling_algo = FCFS;
            }
            else if (strcmp(argv[i], "SJF") == 0)
            {
                args->scheduling_algo = SJF;
            }
        }
        // Parse -i INFILE (input file for burst info)
        else if (strcmp(argv[i], "-i") == 0 && i + 1 < argc)
        {
            args->input_file = argv[++i];
        }
        // Parse -r T T1 T2 L L1 L2 PC (random burst generation)
        else if (strcmp(argv[i], "-r") == 0 && i + 7 < argc)
        {
            args->random_generation = 1;
            args->T = atoi(argv[++i]); // Mean interarrival time
            args->T1 = atoi(argv[++i]);
            args->T2 = atoi(argv[++i]);
            args->L = atoi(argv[++i]); // Mean burst length
            args->L1 = atoi(argv[++i]);
            args->L2 = atoi(argv[++i]);
            args->PC = atoi(argv[++i]); // Number of bursts to generate
        }
    }

    return 1; // Parsing successful
}

// Function to read bursts from input file
void read_bursts_from_file(char *filename)
{
    FILE *file = fopen(filename, "r");
    if (!file)
    {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    char line[100];
    int process_length;
    int interarrival_time;

    while (fgets(line, sizeof(line), file))
    {
        if (sscanf(line, "PL %d", &process_length) == 1)
        {
            burst_t burst = create_burst(process_length); // Assume create_burst() function exists
            add_burst_to_single_queue(burst);             // Or multi-queue if required
        }
        else if (sscanf(line, "IAT %d", &interarrival_time) == 1)
        {
            usleep(interarrival_time * 1000); // Simulate interarrival time with usleep
        }
    }

    fclose(file);
}

// Function to generate random bursts using specified parameters
void generate_random_bursts(arguments args)
{
    for (int i = 0; i < args.PC; i++)
    {
        // Generate random interarrival time
        int interarrival_time = generate_random_time(args.T, args.T1, args.T2);

        // Generate random burst length
        int burst_length = generate_random_time(args.L, args.L1, args.L2);

        // Create burst and add to the queue
        burst_t burst = create_burst(burst_length); // Assume create_burst() function exists
        if (args.multi_queue)
        {
            add_burst_to_multi_queue(burst, RM); // Default method is Round-Robin (for now)
        }
        else
        {
            add_burst_to_single_queue(burst); // Single-queue FCFS or SJF
        }

        usleep(interarrival_time * 1000); // Simulate interarrival time
    }
}

// Helper function to generate a random time based on an exponential distribution
int generate_random_time(int mean, int min, int max)
{
    double lambda = 1.0 / mean;
    double random_value = -log(1.0 - (double)rand() / RAND_MAX) / lambda;
    int random_time = (int)random_value;

    // Clamp the random value within the specified range
    if (random_time < min)
        random_time = min;
    if (random_time > max)
        random_time = max;

    return random_time;
}
