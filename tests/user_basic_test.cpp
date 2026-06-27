#include "test_helpers.h"

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

TEST_CASE("Account type is correctly set", "[account_type]") {
    User u = createTestUser("Typed User", CURRENT);
    REQUIRE(u.getAccountType() == CURRENT);
}
