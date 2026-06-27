#pragma once
#include "catch.hpp"
#include "../include/user.h"
#include "../include/user_dao.h"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

// Guard for restoring current working directory
struct CwdGuard {
    fs::path original;
    CwdGuard() : original(fs::current_path()) {}
    ~CwdGuard() {
        std::error_code ec;
        fs::current_path(original, ec);
    }
};

// Prepare test workspace with isolated directory
[[maybe_unused]] inline fs::path prepareWorkspace(const std::string& name) {
    fs::path root = fs::current_path() / "test_tmp" / name;
    std::error_code ec;
    fs::remove_all(root, ec);
    ec.clear();
    fs::create_directories(root, ec);
    return root;
}

// Guard for redirecting stdin
struct CinRedirectGuard {
    std::streambuf* original;
    explicit CinRedirectGuard(std::istream& in, std::streambuf* replacement) : original(in.rdbuf(replacement)) {}
    ~CinRedirectGuard() {
        std::cin.rdbuf(original);
    }
};

// Guard for redirecting stdout
struct CoutRedirectGuard {
    std::streambuf* original;
    explicit CoutRedirectGuard(std::ostream& out, std::streambuf* replacement) : original(out.rdbuf(replacement)) {}
    ~CoutRedirectGuard() {
        std::cout.rdbuf(original);
    }
};

// Guard for time override testing
struct TimeOverrideGuard {
    explicit TimeOverrideGuard(int64_t epochSeconds) {
        User::setTimeOverrideForTesting(epochSeconds);
    }
    ~TimeOverrideGuard() {
        User::clearTimeOverrideForTesting();
    }
};

// Utility to create a basic user without triggering user input
inline User createTestUser(const std::string& name = "Test User", Type type = SAVINGS, double balance = 0.0) {
    User user;
    user.setUserName(name);
    user.setAccountType(type);
    user.setBalance(balance);
    return user;
}
