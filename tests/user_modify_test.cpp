#include "test_helpers.h"

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
