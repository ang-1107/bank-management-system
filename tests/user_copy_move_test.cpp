#include "test_helpers.h"

TEST_CASE("Copy constructor preserves user data and behavior state", "[copy_semantics]") {
    TimeOverrideGuard timeGuard(1700600000);

    User original = createTestUser("Copy Test", SAVINGS, 500000.0);
    original.setPassword("copy-pass");
    original.deposit(50000.0);
    REQUIRE(original.getCurrent24hVolume() == Approx(50000.0));

    User copy = original;

    REQUIRE(copy.getUserName() == original.getUserName());
    REQUIRE(copy.getBalance() == original.getBalance());
    REQUIRE(copy.getAccountType() == original.getAccountType());
    REQUIRE(copy.getCurrent24hVolume() == Approx(50000.0));
    REQUIRE(copy.verifyPassword("copy-pass"));
}

TEST_CASE("Copy assignment deep copies behavior state", "[copy_semantics]") {
    TimeOverrideGuard timeGuard(1700700000);

    User source = createTestUser("Source", SAVINGS, 300000.0);
    source.setPassword("source-pass");
    source.deposit(80000.0);

    User dest = createTestUser("Dest", CURRENT, 100.0);
    dest = source;

    REQUIRE(dest.getUserName() == "Source");
    REQUIRE(dest.getBalance() == Approx(380000.0));
    REQUIRE(dest.getAccountType() == SAVINGS);
    REQUIRE(dest.getCurrent24hVolume() == Approx(80000.0));
    REQUIRE(dest.verifyPassword("source-pass"));
}

TEST_CASE("Move constructor transfers ownership of behavior", "[move_semantics]") {
    TimeOverrideGuard timeGuard(1700800000);

    User original = createTestUser("Move Source", SAVINGS, 250000.0);
    original.setPassword("move-pass");
    original.deposit(75000.0);

    User moved = std::move(original);

    REQUIRE(moved.getUserName() == "Move Source");
    REQUIRE(moved.getBalance() == Approx(325000.0));
    REQUIRE(moved.getCurrent24hVolume() == Approx(75000.0));
    REQUIRE(moved.verifyPassword("move-pass"));
}

TEST_CASE("Move assignment transfers ownership of behavior", "[move_semantics]") {
    TimeOverrideGuard timeGuard(1700900000);

    User source = createTestUser("Move Assign Source", SAVINGS, 400000.0);
    source.setPassword("assign-pass");
    source.deposit(60000.0);

    User dest = createTestUser("Move Assign Dest", CURRENT, 50.0);
    dest = std::move(source);

    REQUIRE(dest.getUserName() == "Move Assign Source");
    REQUIRE(dest.getBalance() == Approx(460000.0));
    REQUIRE(dest.getAccountType() == SAVINGS);
    REQUIRE(dest.getCurrent24hVolume() == Approx(60000.0));
}

TEST_CASE("Behavior switching on type change preserves balance", "[behavior_switching]") {
    TimeOverrideGuard timeGuard(1708000000);

    User u = createTestUser("Behavior Switch", SAVINGS, 350000.0);
    u.setPassword("switch-pass");
    u.deposit(70000.0);
    REQUIRE(u.getCurrent24hVolume() == Approx(70000.0));

    // Switch to Current
    u.setAccountType(CURRENT);
    REQUIRE(u.getAccountType() == CURRENT);
    REQUIRE(u.getBalance() == Approx(420000.0));
    REQUIRE(u.getCurrent24hVolume() == Approx(0.0));

    // Current behavior should allow unlimited deposits
    u.deposit(100000.0);
    REQUIRE(u.getBalance() == Approx(520000.0));

    // Switch back to Savings
    u.setAccountType(SAVINGS);
    REQUIRE(u.getAccountType() == SAVINGS);
    REQUIRE(u.getBalance() == Approx(520000.0));
    REQUIRE(u.getCurrent24hVolume() == Approx(0.0));
}

TEST_CASE("SavingsBehavior blocks transactions exceeding rolling window", "[behavior_unit]") {
    TimeOverrideGuard timeGuard(1708100000);

    User u = createTestUser("SavingsBehavior Test", SAVINGS, 150000.0);
    u.setPassword("savings-pass");

    // First deposit: 50000 (within 100000 limit)
    u.deposit(50000.0);
    REQUIRE(u.getBalance() == Approx(200000.0));
    REQUIRE(u.getCurrent24hVolume() == Approx(50000.0));

    // Second deposit: another 50000 (total 100000, still within limit)
    u.deposit(50000.0);
    REQUIRE(u.getBalance() == Approx(250000.0));
    REQUIRE(u.getCurrent24hVolume() == Approx(100000.0));

    // Third deposit: 1000 (would exceed 100000 limit, should be blocked)
    u.deposit(1000.0);
    REQUIRE(u.getBalance() == Approx(250000.0));
    REQUIRE(u.getCurrent24hVolume() == Approx(100000.0));
}

TEST_CASE("CurrentBehavior allows unlimited rolling volume", "[behavior_unit]") {
    TimeOverrideGuard timeGuard(1708200000);

    User u = createTestUser("CurrentBehavior Test", CURRENT, 100000.0);
    u.setPassword("current-pass");

    // Deposit multiple large amounts (would exceed 100000 for savings)
    for (int i = 0; i < 5; ++i) {
        u.deposit(50000.0);
    }

    REQUIRE(u.getBalance() == Approx(350000.0));
    REQUIRE(u.getCurrent24hVolume() == Approx(0.0));
    REQUIRE(u.getRemaining24hVolume() == Approx(0.0));

    // Withdraw should also work without volume restrictions
    bool result = u.withdraw(100000.0);
    REQUIRE(result);
    REQUIRE(u.getBalance() == Approx(250000.0));
}

TEST_CASE("Savings account rejects withdraw at volume boundary", "[behavior_unit][boundary]") {
    TimeOverrideGuard timeGuard(1708300000);

    User u = createTestUser("Boundary Savings", SAVINGS, 200000.0);
    u.setPassword("boundary-pass");

    // Deposit 100000 (at limit)
    u.deposit(100000.0);
    REQUIRE(u.getBalance() == Approx(300000.0));
    REQUIRE(u.getCurrent24hVolume() == Approx(100000.0));

    // Withdraw 1 (would exceed limit, should be blocked)
    bool result = u.withdraw(1.0);
    REQUIRE_FALSE(result);
    REQUIRE(u.getBalance() == Approx(300000.0));
}

TEST_CASE("Copy of user with multiple transactions preserves full history", "[copy_semantics][integration]") {
    CwdGuard guard;
    fs::path root = prepareWorkspace("copy_with_transactions");
    fs::current_path(root);

    TimeOverrideGuard timeGuard(1708400000);

    User u = createTestUser("Multi Tx Copy", SAVINGS, 500000.0);
    u.setPassword("copy-tx-pass");
    u.deposit(30000.0);
    u.withdraw(10000.0);
    u.deposit(20000.0);

    User copy = u;

    // Verify copy has same transaction state
    REQUIRE(copy.getCurrent24hVolume() == Approx(60000.0));
    REQUIRE(copy.getRemaining24hVolume() == Approx(40000.0));

    // Persist both and verify they maintain independent state
    std::vector<User> users{u, copy};
    REQUIRE(User::persist(users));

    std::vector<User> loaded = User::loadFromCsv();
    REQUIRE(loaded.size() == 2);
    REQUIRE(loaded[0].getCurrent24hVolume() == Approx(60000.0));
    REQUIRE(loaded[1].getCurrent24hVolume() == Approx(60000.0));
}
