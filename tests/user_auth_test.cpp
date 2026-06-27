#include "test_helpers.h"
#include <algorithm>
#include <cctype>

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

TEST_CASE("Auth works with stored hash value", "[auth][regression]") {
    User source;
    source.setPassword("stored-pass");

    User loaded = createTestUser("Hash User", CURRENT, 15.0);
    loaded.setPasswordHash(source.getPasswordHash());

    REQUIRE(loaded.verifyPassword("stored-pass"));
    REQUIRE_FALSE(loaded.verifyPassword("bad-pass"));
}
