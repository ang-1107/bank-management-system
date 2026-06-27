#include "test_helpers.h"

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
    REQUIRE(UserDAO().persistAll(users));

    fs::path txFile = fs::path("data") / "transactions" / "Tx_User_transactions.csv";
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
    REQUIRE(UserDAO().persistAll(std::vector<User>{u}));
    User::clearTimeOverrideForTesting();

    fs::path txFile = fs::path("data") / "transactions" / "No_Tx_Current_transactions.csv";
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
    REQUIRE(UserDAO().persistAll(users));

    std::vector<User> loaded = UserDAO().loadAll();
    REQUIRE(loaded.size() == 1);
    REQUIRE(loaded[0].getCurrent24hVolume() == Approx(25000.0));
    REQUIRE(loaded[0].getRemaining24hVolume() == Approx(75000.0));

    User::clearTimeOverrideForTesting();
}
