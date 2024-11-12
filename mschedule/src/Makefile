# Compiler and flags
CC = gcc
CFLAGS = -Wall -pthread
LDFLAGS = -lm
TARGET = mschedule
 
# Source files
SRCS = mschedule.c

# Object files (generated from source files)
OBJS = $(SRCS:.c=.o)

# Default target to build the executable
all: $(TARGET)

# Rule to build the executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)

# Rule to generate object files from source files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up object files and the executable
clean:
	rm -f $(OBJS) $(TARGET)

# Phony targets prevent make from confusing these with files named "all" or "clean"
.PHONY: all clean
