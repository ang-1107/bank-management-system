#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include "../include/user.h"
#include <algorithm>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

struct CwdGuard {
    fs::path original;
    CwdGuard() : original(fs::current_path()) {}
    ~CwdGuard() {
        std::error_code ec;
        fs::current_path(original, ec);
    }
};

static fs::path prepareWorkspace(const std::string& name) {
    fs::path root = fs::current_path() / "test_tmp" / name;
    std::error_code ec;
    fs::remove_all(root, ec);
    ec.clear();
    fs::create_directories(root, ec);
    return root;
}

// Utility to create a basic user without triggering user input
User createTestUser(const std::string& name = "Test User", Type type = SAVINGS, double balance = 0.0) {
    User user;
    user.setUserName(name);
    user.setAccountType(type);
    user.setBalance(balance);
    return user;
}

TEST_CASE("Deposit increases balance correctly", "[deposit]") {
    User u = createTestUser();
    u.deposit(100.0);
    REQUIRE(u.getBalance() == Approx(100.0));
}

TEST_CASE("Withdraw reduces balance if sufficient", "[withdraw]") {
    User u = createTestUser("Withdraw Test", SAVINGS, 200.0);
    bool result = u.withdraw(150.0);
    REQUIRE(result == true);
    REQUIRE(u.getBalance() == Approx(50.0));
}

TEST_CASE("Withdraw fails with insufficient balance", "[withdraw]") {
    User u = createTestUser("Low Funds", CURRENT, 50.0);
    bool result = u.withdraw(100.0);
    REQUIRE(result == false);
    REQUIRE(u.getBalance() == Approx(50.0));
}

TEST_CASE("Account type is correctly set and exported", "[account_type]") {
    User u = createTestUser("Typed User", CURRENT);
    json j = u.toJson();
    REQUIRE(j["account_type"] == "Current");
}

TEST_CASE("Persist creates data directory and both files", "[integration][io]") {
    CwdGuard guard;
    fs::path root = prepareWorkspace("persist_creates_files");
    fs::current_path(root);

    REQUIRE_FALSE(fs::exists("data"));

    std::vector<User> users;
    REQUIRE(User::persist(users));

    REQUIRE(fs::exists("data"));
    REQUIRE(fs::exists("data/accounts.csv"));
    REQUIRE(fs::exists("data/accounts.json"));
}

TEST_CASE("CSV is startup source and JSON is synchronized", "[integration][startup][sync]") {
    CwdGuard guard;
    fs::path root = prepareWorkspace("csv_startup_sync");
    fs::current_path(root);

    fs::create_directories("data");
    std::ofstream csv("data/accounts.csv");
    REQUIRE(csv.is_open());
    csv << "Account Number,Name,Balance,Type\n";
    csv << "000020260418101,Alice,120.50,Savings\n";
    csv << "000020260418102,Bob,30.00,Current\n";
    csv.close();

    std::vector<User> loaded = User::loadFromCsv();
    REQUIRE(loaded.size() == 2);

    auto byNumber = [&](const std::string& accNo) {
        return std::find_if(loaded.begin(), loaded.end(), [&](const User& u) {
            return u.getAccountNumber() == accNo;
        });
    };

    auto alice = byNumber("000020260418101");
    auto bob = byNumber("000020260418102");
    REQUIRE(alice != loaded.end());
    REQUIRE(bob != loaded.end());
    REQUIRE(alice->getUserName() == "Alice");
    REQUIRE(alice->getBalance() == Approx(120.50));
    REQUIRE(alice->getAccountType() == SAVINGS);
    REQUIRE(bob->getAccountType() == CURRENT);

    std::vector<User> loadedFromJson = User::loadFromJson();
    REQUIRE(loadedFromJson.size() == loaded.size());
}

TEST_CASE("Regression: persisted updates are reloaded consistently", "[regression][persistence]") {
    CwdGuard guard;
    fs::path root = prepareWorkspace("regression_persist_reload");
    fs::current_path(root);

    std::vector<User> users;
    users.push_back(createTestUser("Regression User", SAVINGS, 100.0));
    REQUIRE(User::persist(users));

    users[0].deposit(25.0);
    REQUIRE(User::persist(users));

    std::vector<User> loaded = User::loadFromCsv();
    REQUIRE(loaded.size() == 1);
    REQUIRE(loaded[0].getUserName() == "Regression User");
    REQUIRE(loaded[0].getBalance() == Approx(125.0));

    std::vector<User> loadedFromJson = User::loadFromJson();
    REQUIRE(loadedFromJson.size() == 1);
    REQUIRE(loadedFromJson[0].getBalance() == Approx(125.0));
}

TEST_CASE("CSV parsing supports quoted commas in names", "[integration][csv]") {
    CwdGuard guard;
    fs::path root = prepareWorkspace("csv_quoted_name");
    fs::current_path(root);

    std::vector<User> users;
    users.push_back(createTestUser("Last, First", CURRENT, 77.7));
    REQUIRE(User::persist(users));

    std::vector<User> loaded = User::loadFromCsv();
    REQUIRE(loaded.size() == 1);
    REQUIRE(loaded[0].getUserName() == "Last, First");
    REQUIRE(loaded[0].getBalance() == Approx(77.70));
    REQUIRE(loaded[0].getAccountType() == CURRENT);
}
