# Compiler and flags
CC = gcc
CFLAGS = -Wall -g -pthread  # Enable warnings, debugging, and pthread support

# Output executable name
TARGET = mschedule

# Source files
SRC = main1.c scheduler_fcfs_1.c scheduler_multiqueue_1.c thread_manager_1.c utils_1.c

# Object files (replace .c with .o in the list of source files)
OBJ = $(SRC:.c=.o)

# Default target to build the executable
all: $(TARGET)

# Rule to link the object files and create the final executable
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

# Rule to compile .c files into .o files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up object files and the executable
clean:
	rm -f *.o $(TARGET) *~

