# Compiler and flags
CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -I./include

# Sources and artifacts
MAIN_SRC := src/main.cpp
USER_SRC := src/user.cpp
OBJS := main.o user.o

TARGET := bank.exe
TEST := test.exe

all: $(TARGET)

main.o: $(MAIN_SRC) include/user.h
	$(CXX) $(CXXFLAGS) -c $(MAIN_SRC) -o $@

user.o: $(USER_SRC) include/user.h
	$(CXX) $(CXXFLAGS) -c $(USER_SRC) -o $@

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(TARGET)

test: tests/user_test.cpp $(USER_SRC) include/user.h
	$(CXX) $(CXXFLAGS) tests/user_test.cpp $(USER_SRC) -o $(TEST)
	./$(TEST)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET) $(TEST)
	rm -rf test_tmp

.PHONY: all test run clean
