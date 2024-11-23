# Indicating that "all" and "clean" are not actual files
.PHONY: all clean

# Set some variables
CC = g++
CFLAGS = -pthread -Wall -Wextra -Werror -O3 -Wpedantic -g
OUTPUT_OPTION = -MMD -MP -o $@

SOURCE = sched_demo_313706015.c
OBJS = $(SOURCE:.c=.o)
DEPS = $(SOURCE:.c=.d)
TARGET = sched_demo_313706015

# Default target to build the program
all: $(TARGET)

# Import the dependencies of .h and .c files from the compiler
-include $(DEPS)

# Compile object files from the source files
%.o: %.c
	$(CC) $(CFLAGS) -c $< $(OUTPUT_OPTION)

# Link object files to create the final executable
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@

# Clean up the executable and intermediate files
clean:
	@rm -f $(TARGET) $(OBJS) $(DEPS)
