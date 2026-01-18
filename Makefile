# Compiler
CXX = g++

# Compiler flags
CXXFLAGS = -Wall -Wextra -w -std=c++23 -O

# Include paths
INCLUDEDIRS = -I/usr/local/include -I./include

# Library paths and libraries
LIBDIRS = -L/usr/local/lib -L./libs
LIBS = -lbenchmark -lpthread

# Target executable name
TARGET = llq

# Source files
SRCS = $(wildcard *.cpp)

# Object files (replace .cpp with .o)
OBJS = $(SRCS:.cpp=.o)

# Default target
all: $(TARGET)

# Debug build
debug: CXXFLAGS += -DDEBUG -g
debug: CCFLAGS += -DDEBUG -g
debug: $(TARGET)

# Benchmark build
benchmark: CXXFLAGS += -DBENCHMARK_LLQ
benchmark: CCFLAGS += -DBENCHMARK_LLQ
benchmark: $(TARGET)

# Rule to build the target executable
$(TARGET): $(OBJS)
	@echo "Build $@"
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS) $(LIBDIRS) $(LIBS)

# Rule to build object files from source files
%.o: %.cpp
	@echo "Build $@"
	$(CXX) $(CXXFLAGS) $(INCLUDEDIRS) -c $< -o $@

# Clean up build files
clean:
	rm -rf $(OBJS) $(TARGET)

print-vars:
	@echo "INCLUDES = $(INCLUDEDIRS)"
	@echo "LIBS = $(LIBDIRS)"

# Phony targets (not actual files)
.PHONY: all clean
