#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include "../include/user.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

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

struct CinRedirectGuard {
    std::streambuf* original;
    explicit CinRedirectGuard(std::istream& in, std::streambuf* replacement) : original(in.rdbuf(replacement)) {}
    ~CinRedirectGuard() {
        std::cin.rdbuf(original);
    }
};

struct CoutRedirectGuard {
    std::streambuf* original;
    explicit CoutRedirectGuard(std::ostream& out, std::streambuf* replacement) : original(out.rdbuf(replacement)) {}
    ~CoutRedirectGuard() {
        std::cout.rdbuf(original);
    }
};

struct TimeOverrideGuard {
    explicit TimeOverrideGuard(int64_t epochSeconds) {
        User::setTimeOverrideForTesting(epochSeconds);
    }
    ~TimeOverrideGuard() {
        User::clearTimeOverrideForTesting();
    }
};

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

TEST_CASE("Password is stored as hash and verified on reload", "[regression][auth]") {
    CwdGuard guard;
    fs::path root = prepareWorkspace("password_hash_roundtrip");
    fs::current_path(root);

    std::vector<User> users;
    User u = createTestUser("Secure User", SAVINGS, 40.0);
    u.setPassword("my-secret-password");
    users.push_back(u);

    REQUIRE(User::persist(users));

    std::vector<User> loaded = User::loadFromCsv();
    REQUIRE(loaded.size() == 1);
    REQUIRE(loaded[0].verifyPassword("my-secret-password"));
    REQUIRE_FALSE(loaded[0].verifyPassword("wrong-password"));
    REQUIRE(loaded[0].getPasswordHash() != "my-secret-password");
}

TEST_CASE("JSON fallback is used only when CSV is missing", "[integration][fallback]") {
    CwdGuard guard;
    fs::path root = prepareWorkspace("json_fallback_only");
    fs::current_path(root);

    std::vector<User> users;
    User u = createTestUser("Fallback User", CURRENT, 99.0);
    u.setPassword("fallback-pass");
    users.push_back(u);

    REQUIRE(User::saveToJson(users));
    REQUIRE_FALSE(fs::exists("data/accounts.csv"));

    std::vector<User> loaded = User::loadFromCsv();
    REQUIRE(loaded.size() == 1);
    REQUIRE(loaded[0].getUserName() == "Fallback User");
    REQUIRE(loaded[0].verifyPassword("fallback-pass"));
    REQUIRE(fs::exists("data/accounts.csv"));
}

TEST_CASE("Password policy allows printable ASCII except whitespace/control", "[edge][auth]") {
    REQUIRE(User::isPasswordPolicyValid("AbC123!@#_+-=[]{}|;:',.<>/?`~\\\""));
    REQUIRE_FALSE(User::isPasswordPolicyValid(""));
    REQUIRE_FALSE(User::isPasswordPolicyValid("has space"));
    REQUIRE_FALSE(User::isPasswordPolicyValid("tab\tchar"));
    REQUIRE_FALSE(User::isPasswordPolicyValid("line\nfeed"));
    REQUIRE_FALSE(User::isPasswordPolicyValid("carriage\rreturn"));
}

TEST_CASE("Password hash shape and determinism", "[edge][auth]") {
    User u;
    u.setPassword("Strong!Pass123");
    std::string first = u.getPasswordHash();
    u.setPassword("Strong!Pass123");
    std::string second = u.getPasswordHash();

    REQUIRE(first == second);
    REQUIRE(first.size() == 64);
    REQUIRE(std::all_of(first.begin(), first.end(), [](unsigned char ch) {
        return std::isxdigit(ch) != 0;
    }));

    u.setPassword("Different!Pass123");
    REQUIRE(first != u.getPasswordHash());
}

TEST_CASE("CSV takes precedence over JSON when both exist", "[weird][fallback]") {
    CwdGuard guard;
    fs::path root = prepareWorkspace("csv_precedence_over_json");
    fs::current_path(root);

    fs::create_directories("data");
    {
        std::ofstream csv("data/accounts.csv");
        REQUIRE(csv.is_open());
        csv << "Account Number,Name,Balance,Type,PasswordHash\n";
        User hashUser;
        hashUser.setPassword("csv-pass");
        csv << "000020260418901,CSV User,10.00,Savings," << hashUser.getPasswordHash() << "\n";
    }

    std::vector<User> jsonUsers;
    User jsonUser = createTestUser("JSON User", CURRENT, 999.0);
    jsonUser.setPassword("json-pass");
    jsonUsers.push_back(jsonUser);
    REQUIRE(User::saveToJson(jsonUsers));

    std::vector<User> loaded = User::loadFromCsv();
    REQUIRE(loaded.size() == 1);
    REQUIRE(loaded[0].getUserName() == "CSV User");
    REQUIRE(loaded[0].verifyPassword("csv-pass"));
}

TEST_CASE("Legacy 4-column CSV rows remain readable", "[regression][csv]") {
    CwdGuard guard;
    fs::path root = prepareWorkspace("legacy_csv_row_support");
    fs::current_path(root);

    fs::create_directories("data");
    std::ofstream csv("data/accounts.csv");
    REQUIRE(csv.is_open());
    csv << "Account Number,Name,Balance,Type\n";
    csv << "000020260418777,Legacy User,88.80,Current\n";
    csv.close();

    std::vector<User> loaded = User::loadFromCsv();
    REQUIRE(loaded.size() == 1);
    REQUIRE(loaded[0].getUserName() == "Legacy User");
    REQUIRE(loaded[0].getBalance() == Approx(88.80));
    REQUIRE_FALSE(loaded[0].verifyPassword("anything"));
}

TEST_CASE("Stress: persist and reload many users", "[stress][persistence]") {
    CwdGuard guard;
    fs::path root = prepareWorkspace("stress_many_users");
    fs::current_path(root);

    std::vector<User> users;
    const int totalUsers = 400;
    users.reserve(static_cast<size_t>(totalUsers));

    for (int i = 0; i < totalUsers; ++i) {
        User u = createTestUser("User_" + std::to_string(i), (i % 2 == 0) ? SAVINGS : CURRENT, i * 1.25);
        u.setPassword("P@ss" + std::to_string(i) + "!X");
        users.push_back(u);
    }

    REQUIRE(User::persist(users));

    std::vector<User> loaded = User::loadFromCsv();
    REQUIRE(static_cast<int>(loaded.size()) == totalUsers);
    REQUIRE(loaded.front().verifyPassword("P@ss0!X"));
    REQUIRE(loaded.back().verifyPassword("P@ss399!X"));
    REQUIRE(loaded[123].getBalance() == Approx(123 * 1.25));
}

TEST_CASE("Modify account can update only name", "[modify][edge]") {
    User u = createTestUser("Original Name", SAVINGS, 10.0);

    std::istringstream fakeInput("1\nNew Name\n");
    CinRedirectGuard cinGuard(std::cin, fakeInput.rdbuf());
    u.modifyAccount();

    REQUIRE(u.getUserName() == "New Name");
    REQUIRE(u.getAccountType() == SAVINGS);
}

TEST_CASE("Modify account can update only type", "[modify][edge]") {
    User u = createTestUser("Keep Name", SAVINGS, 10.0);

    std::istringstream fakeInput("2\n1\n");
    CinRedirectGuard cinGuard(std::cin, fakeInput.rdbuf());
    u.modifyAccount();

    REQUIRE(u.getUserName() == "Keep Name");
    REQUIRE(u.getAccountType() == CURRENT);
}

TEST_CASE("Modify account supports invalid menu input then both fields", "[modify][weird]") {
    User u = createTestUser("Old Name", SAVINGS, 10.0);

    std::istringstream fakeInput("9\n3\nNew Both\n1\n");
    CinRedirectGuard cinGuard(std::cin, fakeInput.rdbuf());
    u.modifyAccount();

    REQUIRE(u.getUserName() == "New Both");
    REQUIRE(u.getAccountType() == CURRENT);
}

TEST_CASE("Auth works with stored hash value", "[auth][regression]") {
    User source;
    source.setPassword("stored-pass");

    User loaded = createTestUser("Hash User", CURRENT, 15.0);
    loaded.setPasswordHash(source.getPasswordHash());

    REQUIRE(loaded.verifyPassword("stored-pass"));
    REQUIRE_FALSE(loaded.verifyPassword("bad-pass"));
}

TEST_CASE("24h rolling volume allows boundary and blocks overflow", "[volume][boundary]") {
    TimeOverrideGuard timeGuard(1700000000);

    User u = createTestUser("Limit User", SAVINGS, 300000.0);
    u.setPassword("limit-pass");

    u.deposit(60000.0);
    REQUIRE(u.getBalance() == Approx(360000.0));

    bool firstWithdraw = u.withdraw(40000.0);
    REQUIRE(firstWithdraw);
    REQUIRE(u.getBalance() == Approx(320000.0));
    REQUIRE(u.getCurrent24hVolume() == Approx(100000.0));
    REQUIRE(u.getRemaining24hVolume() == Approx(0.0));

    u.deposit(1.0);  // should be denied by rolling volume limit
    REQUIRE(u.getBalance() == Approx(320000.0));
}

TEST_CASE("Sliding 24h window refreshes with time progression", "[volume][sliding]") {
    User u = createTestUser("Sliding User", SAVINGS, 500000.0);
    u.setPassword("slide-pass");

    User::setTimeOverrideForTesting(1700000000);
    u.deposit(70000.0);
    REQUIRE(u.getCurrent24hVolume() == Approx(70000.0));

    User::setTimeOverrideForTesting(1700000000 + 24 * 60 * 60 + 1);
    // The previous transaction should now be outside the window.
    REQUIRE(u.getCurrent24hVolume() == Approx(0.0));

    u.deposit(70000.0);
    REQUIRE(u.getBalance() == Approx(640000.0));
    REQUIRE(u.getCurrent24hVolume() == Approx(70000.0));

    User::clearTimeOverrideForTesting();
}

TEST_CASE("Per-user transaction file stores serial and epoch", "[integration][transactions_csv]") {
    CwdGuard guard;
    fs::path root = prepareWorkspace("transactions_file_format");
    fs::current_path(root);

    User::setTimeOverrideForTesting(1700000100);
    User u = createTestUser("Tx User", SAVINGS, 100000.0);
    u.setPassword("tx-pass");
    u.deposit(1000.0);
    u.withdraw(500.0);
    User::clearTimeOverrideForTesting();

    std::vector<User> users{u};
    REQUIRE(User::persist(users));

    fs::path txFile = fs::path("data") / "Tx_User_transactions.csv";
    REQUIRE(fs::exists(txFile));

    std::ifstream in(txFile);
    REQUIRE(in.is_open());

    std::string header;
    std::string row1;
    std::string row2;
    std::getline(in, header);
    std::getline(in, row1);
    std::getline(in, row2);

    REQUIRE(header == "SerialNumber,EpochSeconds,Amount");
    REQUIRE(row1.rfind("1,", 0) == 0);
    REQUIRE(row2.rfind("2,", 0) == 0);
}

TEST_CASE("Current account bypasses 24h volume enforcement", "[volume][current]") {
    TimeOverrideGuard timeGuard(1700200000);

    User u = createTestUser("Current User", CURRENT, 300000.0);
    u.setPassword("cur-pass");

    u.deposit(90000.0);
    REQUIRE(u.getBalance() == Approx(390000.0));

    bool w1 = u.withdraw(90000.0);
    REQUIRE(w1);
    REQUIRE(u.getBalance() == Approx(300000.0));

    // Should still pass even if sum(abs(txn)) would exceed 100000 for savings.
    u.deposit(90000.0);
    REQUIRE(u.getBalance() == Approx(390000.0));
    REQUIRE(u.getCurrent24hVolume() == Approx(0.0));
    REQUIRE(u.getRemaining24hVolume() == Approx(0.0));
}

TEST_CASE("Current account does not maintain transaction file", "[integration][current_transactions]") {
    CwdGuard guard;
    fs::path root = prepareWorkspace("current_no_tx_file");
    fs::current_path(root);

    User::setTimeOverrideForTesting(1700210000);
    User u = createTestUser("No Tx Current", CURRENT, 1000.0);
    u.setPassword("no-tx-pass");
    u.deposit(100.0);
    REQUIRE(User::persist(std::vector<User>{u}));
    User::clearTimeOverrideForTesting();

    fs::path txFile = fs::path("data") / "No_Tx_Current_transactions.csv";
    REQUIRE_FALSE(fs::exists(txFile));
}

TEST_CASE("Type switch resets savings volume state", "[volume][type_switch]") {
    TimeOverrideGuard timeGuard(1700300000);

    User u = createTestUser("Switch User", SAVINGS, 300000.0);
    u.setPassword("switch-pass");
    u.deposit(90000.0);
    REQUIRE(u.getCurrent24hVolume() == Approx(90000.0));

    // Savings -> Current should clear volume state.
    u.setAccountType(CURRENT);
    u.setAccountType(SAVINGS);

    REQUIRE(u.getCurrent24hVolume() == Approx(0.0));
    REQUIRE(u.getRemaining24hVolume() == Approx(100000.0));
}

TEST_CASE("Account type change is limited to once per 24 hours", "[modify][cooldown]") {
    User u = createTestUser("Cooldown User", SAVINGS, 100.0);

    User::setTimeOverrideForTesting(1700400000);
    {
        std::istringstream firstInput("2\n1\n");
        std::ostringstream firstOutput;
        CinRedirectGuard cinGuard(std::cin, firstInput.rdbuf());
        CoutRedirectGuard coutGuard(std::cout, firstOutput.rdbuf());
        u.modifyAccount();
        REQUIRE(firstOutput.str().find("Account type updated successfully") != std::string::npos);
    }
    REQUIRE(u.getAccountType() == CURRENT);

    User::setTimeOverrideForTesting(1700400600);
    {
        std::istringstream secondInput("2\n0\n");
        std::ostringstream secondOutput;
        CinRedirectGuard cinGuard(std::cin, secondInput.rdbuf());
        CoutRedirectGuard coutGuard(std::cout, secondOutput.rdbuf());
        u.modifyAccount();
        REQUIRE(secondOutput.str().find("Account type change denied: cooldown active") != std::string::npos);
    }
    REQUIRE(u.getAccountType() == CURRENT);

    User::setTimeOverrideForTesting(1700400000 + 24 * 60 * 60 + 1);
    {
        std::istringstream thirdInput("2\n0\n");
        std::ostringstream thirdOutput;
        CinRedirectGuard cinGuard(std::cin, thirdInput.rdbuf());
        CoutRedirectGuard coutGuard(std::cout, thirdOutput.rdbuf());
        u.modifyAccount();
        REQUIRE(thirdOutput.str().find("Account type updated successfully") != std::string::npos);
    }
    REQUIRE(u.getAccountType() == SAVINGS);

    User::clearTimeOverrideForTesting();
}

TEST_CASE("Type-change cooldown survives persist and reload", "[modify][cooldown][persistence]") {
    CwdGuard guard;
    fs::path root = prepareWorkspace("cooldown_persist_reload");
    fs::current_path(root);

    User u = createTestUser("Cooldown Persist", SAVINGS, 500.0);
    User::setTimeOverrideForTesting(1700500000);
    {
        std::istringstream input("2\n1\n");
        CinRedirectGuard cinGuard(std::cin, input.rdbuf());
        u.modifyAccount();
    }
    REQUIRE(u.getAccountType() == CURRENT);

    REQUIRE(User::persist(std::vector<User>{u}));

    std::vector<User> loaded = User::loadFromCsv();
    REQUIRE(loaded.size() == 1);
    REQUIRE(loaded[0].getAccountType() == CURRENT);

    User::setTimeOverrideForTesting(1700500300);
    {
        std::istringstream blockedInput("2\n0\n");
        std::ostringstream output;
        CinRedirectGuard cinGuard(std::cin, blockedInput.rdbuf());
        CoutRedirectGuard coutGuard(std::cout, output.rdbuf());
        loaded[0].modifyAccount();
        REQUIRE(output.str().find("Account type change denied: cooldown active") != std::string::npos);
    }
    REQUIRE(loaded[0].getAccountType() == CURRENT);

    User::clearTimeOverrideForTesting();
}

TEST_CASE("Reloaded transaction history retains rolling volume", "[regression][transactions]") {
    CwdGuard guard;
    fs::path root = prepareWorkspace("reload_tx_volume");
    fs::current_path(root);

    User::setTimeOverrideForTesting(1700100000);
    User u = createTestUser("Reload User", SAVINGS, 200000.0);
    u.setPassword("reload-pass");
    u.deposit(20000.0);
    u.withdraw(5000.0);

    std::vector<User> users{u};
    REQUIRE(User::persist(users));

    std::vector<User> loaded = User::loadFromCsv();
    REQUIRE(loaded.size() == 1);
    REQUIRE(loaded[0].getCurrent24hVolume() == Approx(25000.0));
    REQUIRE(loaded[0].getRemaining24hVolume() == Approx(75000.0));

    User::clearTimeOverrideForTesting();
}
