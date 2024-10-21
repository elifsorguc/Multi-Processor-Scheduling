#include <stdio.h>
#include <stdlib.h>
#include "scheduler_fcfs.h"
#include "scheduler_multiqueue.h"
#include "thread_manager.h"
#include "utils.h"

int main(int argc, char *argv[]) {
    // Parse command-line arguments
    arguments args;
    if (!parse_arguments(argc, argv, &args)) {
        printf("Invalid arguments. Exiting.\n");
        return EXIT_FAILURE;
    }

    // Initialize ready queue(s) based on the scheduling method
    if (args.multi_queue) {
        initialize_multi_queues(args.num_processors);  // Multi-queue scheduling (Phase 2)
    } else {
        initialize_single_queue();  // Single-queue for FCFS (Phase 1)
    }

    // Create processor threads for the simulation
    create_processor_threads(args.num_processors, args.scheduling_algo);  // FCFS or SJF

    // Read burst input from file or generate random bursts
    if (args.input_file) {
        read_bursts_from_file(args.input_file);
    } else {
        generate_random_bursts(args);
    }

    // Wait for all processor threads to finish
    wait_for_threads_to_finish();

    // Print final report
    if (args.multi_queue) {
        print_final_report_multi();  // Detailed final report for multi-queue (Phase 2)
    } else {
        print_final_report();  // Final report for single-queue FCFS (Phase 1)
    }

    return EXIT_SUCCESS;
}
