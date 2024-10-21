# Compiler and flags
CC = gcc
CFLAGS = -Wall -g -pthread  # Enable warnings, debugging, and pthread support

# Output executable name
TARGET = mschedule

# Source files
SRC = main.c scheduler_fcfs.c scheduler_multiqueue.c thread_manager.c utils.c

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

