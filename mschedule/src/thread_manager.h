#ifndef THREAD_MANAGER_H
#define THREAD_MANAGER_H

// Function prototypes for thread management
void create_processor_threads(int num_processors, scheduling_algorithm algo);
void wait_for_threads_to_finish();

#endif
