# Compiler and flags
CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -I./include

# Sources and artifacts
MAIN_SRC := src/main.cpp
USER_SRC := src/user.cpp
UTIL_SRC := src/util.cpp
BEHAVIOR_SRC := src/account_behavior.cpp
USER_DAO_SRC := src/user_dao.cpp
OBJS := main.o user.o util.o account_behavior.o user_dao.o

TARGET := bank.exe
TEST := test.exe

all: $(TARGET)

main.o: $(MAIN_SRC) include/user.h include/user_dao.h
	$(CXX) $(CXXFLAGS) -c $(MAIN_SRC) -o $@

user.o: $(USER_SRC) include/user.h
	$(CXX) $(CXXFLAGS) -c $(USER_SRC) -o $@

user_dao.o: $(USER_DAO_SRC) include/user_dao.h include/user.h include/util.h
	$(CXX) $(CXXFLAGS) -c $(USER_DAO_SRC) -o $@

util.o: $(UTIL_SRC) include/util.h
	$(CXX) $(CXXFLAGS) -c $(UTIL_SRC) -o $@

account_behavior.o: $(BEHAVIOR_SRC) include/account_behavior.h include/user.h
	$(CXX) $(CXXFLAGS) -c $(BEHAVIOR_SRC) -o $@

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(TARGET)

test: tests/user_test_main.cpp tests/user_basic_test.cpp tests/user_persistence_test.cpp tests/user_auth_test.cpp tests/user_modify_test.cpp tests/user_volume_test.cpp tests/user_copy_move_test.cpp $(USER_SRC) $(UTIL_SRC) $(BEHAVIOR_SRC) $(USER_DAO_SRC) tests/test_helpers.h include/user.h include/user_dao.h include/util.h include/account_behavior.h
	$(CXX) $(CXXFLAGS) tests/user_test_main.cpp tests/user_basic_test.cpp tests/user_persistence_test.cpp tests/user_auth_test.cpp tests/user_modify_test.cpp tests/user_volume_test.cpp tests/user_copy_move_test.cpp $(USER_SRC) $(UTIL_SRC) $(BEHAVIOR_SRC) $(USER_DAO_SRC) -o $(TEST)
	./$(TEST)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET) $(TEST)
	rm -rf test_tmp

.PHONY: all test run clean
