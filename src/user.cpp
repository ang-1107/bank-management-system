#include "user.h"
#include "util.h"
#include "account_behavior.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cmath>
#include <limits>
#include <sstream>

using namespace std;

const unordered_map<Type, string> accountTypeMap = {
    {SAVINGS, "Savings"},
    {CURRENT, "Current"}
};

int User::counter = 0;                              // Initializing the static counter
int64_t User::time_override_epoch_seconds = -1;

namespace {
constexpr int64_t kTypeChangeCooldownSeconds = 24LL * 60LL * 60LL;

bool isAllowedPasswordChar(unsigned char ch) {
    // Allow only printable ASCII characters excluding spaces and control characters.
    return ch >= 33U && ch <= 126U;
}
}  // namespace

// Generates current date in YYYYMMDD format
string User::getCurrentDate() {
    time_t currentTime = time(0);
    char dateStr[9];
    strftime(dateStr, sizeof(dateStr), "%Y%m%d", localtime(&currentTime));
    return string(dateStr);
}

int64_t User::getCurrentEpochSeconds() {
    if (time_override_epoch_seconds >= 0) {
        return time_override_epoch_seconds;
    }

    const auto currentTime = chrono::system_clock::now();
    return chrono::duration_cast<chrono::seconds>(currentTime.time_since_epoch()).count();
}

void User::syncAccountCounterFromRecords(const vector<UserRecord>& records) {
    const string todayPrefix = "0000" + getCurrentDate();
    int maxCounter = 0;

    for (const auto& record : records) {
        const string& accountNumber = record.account_number;
        if (accountNumber.rfind(todayPrefix, 0) != 0) {
            continue;
        }

        string suffix = accountNumber.substr(todayPrefix.size());
        if (suffix.empty()) {
            continue;
        }

        bool allDigits = all_of(suffix.begin(), suffix.end(), [](char ch) {
            return isdigit(static_cast<unsigned char>(ch));
        });
        if (!allDigits) {
            continue;
        }

        try {
            int parsed = stoi(suffix);
            maxCounter = max(maxCounter, parsed);
        } catch (...) {
            // Ignore malformed suffix and continue.
        }
    }

    counter = maxCounter;
}

// Helper to initialize behavior based on account type
static AccountBehavior* createBehaviorForType(Type type, std::vector<TransactionRecord>* txns) {
    if (type == SAVINGS) {
        return new SavingsBehavior(txns);
    } else {
        return new CurrentBehavior();
    }
}

// Constructor: Generates a unique account number
User::User() {
    counter += 1;
    account_number = "0000" + getCurrentDate() + to_string(counter);
    user_name = "";
    password_hash = "";
    account_balance = 0.0;
    account_type = SAVINGS;
    last_account_type_change_epoch_seconds = -1;
    behavior = unique_ptr<AccountBehavior>(createBehaviorForType(SAVINGS, &recent_transactions));
}

// Destructor
User::~User() {
    // Empty Deconstructor
}

// Copy constructor (deep copy with behavior cloning)
User::User(const User& other)
    : account_number(other.account_number),
      user_name(other.user_name),
      password_hash(other.password_hash),
      account_balance(other.account_balance),
      account_type(other.account_type),
      last_account_type_change_epoch_seconds(other.last_account_type_change_epoch_seconds),
      recent_transactions(other.recent_transactions),
            behavior(nullptr) {
        behavior = unique_ptr<AccountBehavior>(createBehaviorForType(account_type, &recent_transactions));
}

// Copy assignment operator (deep copy with behavior cloning)
User& User::operator=(const User& other) {
    if (this != &other) {
        account_number = other.account_number;
        user_name = other.user_name;
        password_hash = other.password_hash;
        account_balance = other.account_balance;
        account_type = other.account_type;
        last_account_type_change_epoch_seconds = other.last_account_type_change_epoch_seconds;
        recent_transactions = other.recent_transactions;
        behavior = unique_ptr<AccountBehavior>(createBehaviorForType(account_type, &recent_transactions));
    }
    return *this;
}

// Move constructor
User::User(User&& other) noexcept
    : account_number(std::move(other.account_number)),
      user_name(std::move(other.user_name)),
      password_hash(std::move(other.password_hash)),
      account_balance(other.account_balance),
      account_type(other.account_type),
      last_account_type_change_epoch_seconds(other.last_account_type_change_epoch_seconds),
      recent_transactions(std::move(other.recent_transactions)),
      behavior(nullptr) {
    // Recreate behavior pointing to this User's recent_transactions vector.
    behavior = unique_ptr<AccountBehavior>(createBehaviorForType(account_type, &recent_transactions));
}

// Move assignment operator
User& User::operator=(User&& other) noexcept {
    if (this != &other) {
        account_number = std::move(other.account_number);
        user_name = std::move(other.user_name);
        password_hash = std::move(other.password_hash);
        account_balance = other.account_balance;
        account_type = other.account_type;
        last_account_type_change_epoch_seconds = other.last_account_type_change_epoch_seconds;
        recent_transactions = std::move(other.recent_transactions);
        // Recreate behavior pointing to this User's recent_transactions vector.
        behavior = unique_ptr<AccountBehavior>(createBehaviorForType(account_type, &recent_transactions));
    }
    return *this;
}

// Creates a new account with user input
void User::createAccount() {
    cout << "Enter Your Name: ";
    getline(cin >> ws, user_name);  // Allows spaces, prevents `\n` issues

    while (user_name.empty()) {
        cout << "Name cannot be empty. Enter Your Name: ";
        getline(cin >> ws, user_name);
    }

    int accTypeInput;
    cout << "Enter Account Type (0 for Savings, 1 for Current): ";
    while (!(cin >> accTypeInput) || (accTypeInput != 0 && accTypeInput != 1)) {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "Invalid choice! Enter 0 for Savings or 1 for Current: ";
    }

    cin.ignore(numeric_limits<streamsize>::max(), '\n');

    account_type = static_cast<Type>(accTypeInput);
    behavior = unique_ptr<AccountBehavior>(createBehaviorForType(account_type, &recent_transactions));

    string password;
    string confirmPassword;
    while (true) {
        cout << "Set Password: ";
        getline(cin, password);

        if (!isPasswordPolicyValid(password)) {
            cout << "Invalid password! Use printable ASCII except spaces and control characters." << endl;
            continue;
        }

        cout << "Confirm Password: ";
        getline(cin, confirmPassword);
        if (confirmPassword != password) {
            cout << "Passwords do not match. Try again." << endl;
            continue;
        }

        break;
    }

    setPassword(password);
    cout << "Account " << account_number << " created Successfully!" << endl;
}

// Displays account details
void User::displayAccount() const {
    cout << "Account Number: " << account_number << endl;
    cout << "Name: " << user_name << endl;
    cout << "Balance: $" << fixed << setprecision(2) << account_balance << endl;
    cout << "Type: " << accountTypeMap.at(account_type) << endl;
}

// Make a deposit into the account
void User::deposit(double amount) {
    if (amount <= 0) {
        cout << "Invalid amount!" << endl;
        return;
    }

    const int64_t nowEpochSeconds = getCurrentEpochSeconds();
    
    if (!behavior->canApplyDeposit(amount, nowEpochSeconds)) {
        const double remaining = behavior->getRemainingVolume(nowEpochSeconds);
        cout << "Transaction denied: 24-hour volume limit exceeded." << endl;
        cout << "Remaining 24-hour volume: $" << fixed << setprecision(2) << remaining << endl;
        return;
    }

    account_balance += amount;
    behavior->recordTransaction(amount, nowEpochSeconds);
    cout << "Credited $" << fixed << setprecision(2) << amount << " successfully." << endl;
    cout << "Updated Balance: $" << fixed << setprecision(2) << account_balance << endl;
}

// Withdraw money if there is sufficient balance
bool User::withdraw(double amount) {
    if (amount <= 0) {
        cout << "Insufficient balance or invalid amount!" << endl;
        return false;
    }

    if (amount > account_balance) {
        cout << "Insufficient balance or invalid amount!" << endl;
        return false;
    }

    const int64_t nowEpochSeconds = getCurrentEpochSeconds();

    if (!behavior->canApplyWithdraw(amount, nowEpochSeconds)) {
        const double remaining = behavior->getRemainingVolume(nowEpochSeconds);
        cout << "Transaction denied: 24-hour volume limit exceeded." << endl;
        cout << "Remaining 24-hour volume: $" << fixed << setprecision(2) << remaining << endl;
        return false;
    }

    account_balance -= amount;
    behavior->recordTransaction(-amount, nowEpochSeconds);
    cout << "Debited $" << fixed << setprecision(2) << amount << " successfully." << endl;
    cout << "Updated Balance: $" << fixed << setprecision(2) << account_balance << endl;
    return true;
}

// Modify existing account
void User::modifyAccount() {
    cout << "Modify Account Details" << endl;
    cout << "Current Name: " << user_name << endl;
    cout << "Current Account Type: " << accountTypeMap.at(account_type) << endl;
    cout << "Choose what to modify:\n";
    cout << "1. Name\n";
    cout << "2. Account Type\n";
    cout << "3. Both\n";
    cout << "Enter your choice: ";

    int modifyChoice;
    while (!(cin >> modifyChoice) || (modifyChoice < 1 || modifyChoice > 3)) {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "Invalid choice! Enter 1, 2, or 3: ";
    }

    cin.ignore(numeric_limits<streamsize>::max(), '\n');

    if (modifyChoice == 1 || modifyChoice == 3) {
        const string oldName = user_name;
        cout << "Enter New Name: ";
        getline(cin >> ws, user_name);

        while (user_name.empty()) {
            cout << "Name cannot be empty. Enter New Name: ";
            getline(cin >> ws, user_name);
        }

        if (user_name == oldName) {
            cout << "Name unchanged." << endl;
        } else {
            cout << "Name updated successfully." << endl;
        }
    }

    if (modifyChoice == 2 || modifyChoice == 3) {
        int accTypeInput;
        cout << "Enter New Type (0 for Savings, 1 for Current): ";
        while (!(cin >> accTypeInput) || (accTypeInput != 0 && accTypeInput != 1)) {
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            cout << "Invalid choice! Enter 0 for Savings or 1 for Current: ";
        }

        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        const Type requestedType = static_cast<Type>(accTypeInput);
        if (requestedType == account_type) {
            cout << "Account type unchanged." << endl;
        } else {
            const int64_t nowEpochSeconds = getCurrentEpochSeconds();
            const bool cooldownActive = (last_account_type_change_epoch_seconds >= 0) &&
                ((nowEpochSeconds - last_account_type_change_epoch_seconds) < kTypeChangeCooldownSeconds);

            if (cooldownActive) {
                const int64_t elapsed = nowEpochSeconds - last_account_type_change_epoch_seconds;
                const int64_t remaining = kTypeChangeCooldownSeconds - elapsed;
                cout << "Account type change denied: cooldown active. Try again in "
                     << formatCooldownTime(remaining) << "." << endl;
            } else {
                account_type = requestedType;
                last_account_type_change_epoch_seconds = nowEpochSeconds;
                behavior->onAccountTypeChanged();
                behavior = unique_ptr<AccountBehavior>(createBehaviorForType(requestedType, &recent_transactions));
                cout << "Account type updated successfully. Next change allowed after 24 hours." << endl;
            }
        }
    }
}

User User::fromRecord(const UserRecord& record) {
    User user;
    user.account_number = record.account_number;
    user.user_name = record.user_name;
    user.password_hash = record.password_hash;
    user.account_balance = record.account_balance;
    user.account_type = record.account_type;
    user.last_account_type_change_epoch_seconds = record.last_account_type_change_epoch_seconds;
    user.recent_transactions = record.recent_transactions;
    user.behavior = unique_ptr<AccountBehavior>(createBehaviorForType(user.account_type, &user.recent_transactions));
    return user;
}

UserRecord User::toRecord() const {
    UserRecord record;
    record.account_number = account_number;
    record.user_name = user_name;
    record.password_hash = password_hash;
    record.account_balance = account_balance;
    record.account_type = account_type;
    record.last_account_type_change_epoch_seconds = last_account_type_change_epoch_seconds;
    record.recent_transactions = recent_transactions;
    return record;
}

std::string User::getAccountNumber() const {
    return account_number;
}

std::string User::getUserName() const {
    return user_name;
}

std::string User::getPasswordHash() const {
    return password_hash;
}

Type User::getAccountType() const {
    return account_type;
}

double User::getCurrent24hVolume() const {
    return behavior->getCurrentVolume(getCurrentEpochSeconds());
}

double User::getRemaining24hVolume() const {
    return behavior->getRemainingVolume(getCurrentEpochSeconds());
}

void User::setTimeOverrideForTesting(int64_t epochSeconds) {
    time_override_epoch_seconds = epochSeconds;
}

void User::clearTimeOverrideForTesting() {
    time_override_epoch_seconds = -1;
}

bool User::verifyPassword(const std::string& plainPassword) const {
    return password_hash == sha256(plainPassword);
}

void User::setPassword(const std::string& plainPassword) {
    password_hash = sha256(plainPassword);
}

bool User::isPasswordPolicyValid(const std::string& plainPassword) {
    if (plainPassword.empty()) {
        return false;
    }

    for (unsigned char ch : plainPassword) {
        if (!isAllowedPasswordChar(ch)) {
            return false;
        }
    }

    return true;
}

// Get-Set Methods for Testing
void User::setUserName(const std::string& name) {
    this->user_name = name;
    return;
}

void User::setAccountType(Type type) {
    if (this->account_type != type) {
        this->account_type = type;
        behavior->onAccountTypeChanged();
        behavior = unique_ptr<AccountBehavior>(createBehaviorForType(type, &recent_transactions));
    }
    return;
}

void User::setPasswordHash(const std::string& hashValue) {
    this->password_hash = hashValue;
    return;
}

void User::setBalance(double balance) {
    this->account_balance = balance;
    return;
}

double User::getBalance() const {
    return this->account_balance;
}



