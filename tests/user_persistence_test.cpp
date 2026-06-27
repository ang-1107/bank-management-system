#include "test_helpers.h"
#include <algorithm>

TEST_CASE("Persist creates data directory and CSV file", "[integration][io]") {
    CwdGuard guard;
    fs::path root = prepareWorkspace("persist_creates_files");
    fs::current_path(root);

    REQUIRE_FALSE(fs::exists("data"));

    std::vector<User> users;
    REQUIRE(User::persist(users));

    REQUIRE(fs::exists("data"));
    REQUIRE(fs::exists("data/accounts.csv"));
}

TEST_CASE("CSV is startup source of truth", "[integration][startup]") {
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
