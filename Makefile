# Compiler and Flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -I./include

# Source and Object Files
MAIN_SRC = main.cpp
USER_SRC = src/user.cpp
OBJS = main.o user.o

# Executables
TARGET = bank.exe
TEST = user_tests.exe

# Default Rule
all: $(TARGET)

# Compile main.cpp
main.o: $(MAIN_SRC) include/user.h
	$(CXX) $(CXXFLAGS) -c $(MAIN_SRC) -o main.o

# Compile user.cpp
user.o: $(USER_SRC) include/user.h include/json.hpp
	$(CXX) $(CXXFLAGS) -c $(USER_SRC) -o user.o

# Link Object Files to Create Executable
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(TARGET)

# Clean Build Files and Data
clean:
	rm -f $(OBJS) $(TARGET) data/*.json data/*.csv

# Run the Program
run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run

# Test build
test: tests/user_test.cpp src/user.cpp
	$(CXX) $(CXXFLAGS) tests/user_test.cpp src/user.cpp -o $(TEST)
	./$(TEST)

clean:
	rm -f $(TARGET) $(TEST)
