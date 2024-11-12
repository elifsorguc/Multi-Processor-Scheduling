#include <stdio.h>
#include <stdlib.h>
#include "scheduler_fcfs.h"
#include "scheduler_multiqueue.h"
#include "thread_manager.h"
#include "utils.h"

int main(int argc, char *argv[])
{
    // Initialize simulation start time for consistent timestamping
    init_simulation_time();

    // Parse command-line arguments
    arguments args;
    if (!parse_arguments(argc, argv, &args))
    {
        printf("Invalid arguments. Exiting.\n");
        return EXIT_FAILURE;
    }

    // Initialize ready queue(s) based on the scheduling method
    if (args.multi_queue)
    {
        // Multi-queue scheduling (each processor has its own queue)
        initialize_multi_queues(args.num_processors);
    }
    else
    {
        // Single-queue scheduling (common queue for all processors)
        initialize_single_queue();
    }

    // Create processor threads for the simulation
    create_processor_threads(args.num_processors, args.scheduling_algo); // Either FCFS or SJF

    // Read burst input from file or generate random bursts
    if (args.input_file)
    {
        printf("Reading bursts from file: %s\n", args.input_file);
        read_bursts_from_file(args.input_file, args.multi_queue);
    }
    else
    {
        printf("Generating random bursts based on parameters.\n");
        generate_random_bursts(args);
    }

    // Wait for all processor threads to finish processing
    wait_for_threads_to_finish();

    // Print the final report based on the scheduling approach used
    if (args.multi_queue)
    {
        print_final_report_multi(); // Detailed report for multi-queue (Phase 2)
    }
    else
    {
        print_final_report(); // Final report for single-queue FCFS (Phase 1)
    }

    return EXIT_SUCCESS;
}