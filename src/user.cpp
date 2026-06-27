#include "user.h"
#include "util.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <filesystem>
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
const char* kDataDir = "data";
const char* kTransactionsDir = "data/transactions";
const char* kCsvPath = "data/accounts.csv";
const char* kPasswordHeader = "PasswordHash";
const char* kTypeChangeEpochHeader = "LastTypeChangeEpochSeconds";
const char* kTransactionHeader = "SerialNumber,EpochSeconds,Amount";
const char* kTransactionsSuffix = "_transactions.csv";
constexpr int64_t kRollingWindowSeconds = 24LL * 60LL * 60LL;
constexpr int64_t kTypeChangeCooldownSeconds = 24LL * 60LL * 60LL;
constexpr double kDailyTransactionLimit = 100000.0;

bool isAllowedPasswordChar(unsigned char ch) {
    // Allow only printable ASCII characters excluding spaces and control characters.
    return ch >= 33U && ch <= 126U;
}
}  // namespace

// Generates current date in YYYYMMDD format
string User::getCurrentDate() {
    time_t now = time(0);
    char dateStr[9];
    strftime(dateStr, sizeof(dateStr), "%Y%m%d", localtime(&now));
    return string(dateStr);
}

int64_t User::getCurrentEpochSeconds() {
    if (time_override_epoch_seconds >= 0) {
        return time_override_epoch_seconds;
    }

    const auto now = chrono::system_clock::now();
    return chrono::duration_cast<chrono::seconds>(now.time_since_epoch()).count();
}

std::string User::sanitizeUserNameForFile(const std::string& userName) {
    string sanitized;
    sanitized.reserve(userName.size());

    for (unsigned char ch : userName) {
        if (isalnum(ch) != 0) {
            sanitized.push_back(static_cast<char>(ch));
        } else {
            sanitized.push_back('_');
        }
    }

    if (sanitized.empty()) {
        return "user";
    }

    return sanitized;
}

std::string User::transactionFilePath() const {
    return string(kTransactionsDir) + "/" + sanitizeUserNameForFile(user_name) + kTransactionsSuffix;
}

bool User::loadTransactionsForUser(User& user) {
    user.recent_transactions.clear();

    ifstream inFile(user.transactionFilePath());
    if (!inFile.is_open()) {
        return true;
    }

    string line;
    size_t lineNumber = 0;
    while (getline(inFile, line)) {
        ++lineNumber;
        if (line.empty()) {
            continue;
        }

        if (lineNumber == 1 && line.rfind("SerialNumber", 0) == 0) {
            continue;
        }

        vector<string> fields = splitCsvLine(line);
        if (fields.size() != 3) {
            cerr << "Warning: Skipping malformed transaction row for user "
                 << user.user_name << " at line " << lineNumber << "." << endl;
            continue;
        }

        try {
            TransactionRecord record;
            record.epoch_seconds = stoll(fields[1]);
            record.amount = stod(fields[2]);
            user.recent_transactions.push_back(record);
        } catch (const exception&) {
            cerr << "Warning: Skipping invalid transaction row for user "
                 << user.user_name << " at line " << lineNumber << "." << endl;
        }
    }

    return true;
}

bool User::saveTransactionsForUser(const User& user) {
    if (!ensureDataDirectory()) {
        return false;
    }

    std::error_code ec;
    std::filesystem::create_directories(kTransactionsDir, ec);
    if (ec) {
        cerr << "Error: Could not create transactions directory: " << ec.message() << endl;
        return false;
    }

    ofstream outFile(user.transactionFilePath(), ios::trunc);
    if (!outFile.is_open()) {
        perror("Error opening per-user transactions CSV");
        cerr << "Error: Could not write transactions for user " << user.user_name << "." << endl;
        return false;
    }

    outFile << kTransactionHeader << "\n";
    for (size_t i = 0; i < user.recent_transactions.size(); ++i) {
        const auto& record = user.recent_transactions[i];
        outFile << (i + 1) << "," << record.epoch_seconds << "," << record.amount << "\n";
    }

    if (!outFile.good()) {
        perror("Error writing per-user transactions CSV");
        cerr << "Error: Failed while writing transactions for user " << user.user_name << "." << endl;
        return false;
    }

    return true;
}

bool User::saveTransactionsForUsers(const vector<User>& users) {
    bool allOk = true;
    for (const auto& user : users) {
        if (user.account_type == SAVINGS) {
            if (!saveTransactionsForUser(user)) {
                allOk = false;
            }
        } else {
            std::error_code ec;
            std::filesystem::remove(user.transactionFilePath(), ec);
        }
    }
    return allOk;
}

void User::pruneExpiredTransactions(int64_t nowEpochSeconds) {
    const int64_t cutoff = nowEpochSeconds - kRollingWindowSeconds;
    recent_transactions.erase(
        remove_if(recent_transactions.begin(), recent_transactions.end(), [&](const TransactionRecord& record) {
            return record.epoch_seconds < cutoff;
        }),
        recent_transactions.end());
}

double User::rolling24hVolume(int64_t nowEpochSeconds) const {
    const int64_t cutoff = nowEpochSeconds - kRollingWindowSeconds;
    double total = 0.0;
    for (const auto& record : recent_transactions) {
        if (record.epoch_seconds >= cutoff) {
            total += fabs(record.amount);
        }
    }
    return total;
}

bool User::canApplyTransaction(double signedAmount, int64_t nowEpochSeconds) const {
    if (account_type != SAVINGS) {
        return true;
    }
    const double currentVolume = rolling24hVolume(nowEpochSeconds);
    return currentVolume + fabs(signedAmount) <= kDailyTransactionLimit;
}

void User::recordTransaction(double signedAmount, int64_t nowEpochSeconds) {
    recent_transactions.push_back(TransactionRecord{nowEpochSeconds, signedAmount});
}

bool User::ensureDataDirectory() {
    std::error_code ec;
    if (std::filesystem::exists(kDataDir, ec)) {
        if (!std::filesystem::is_directory(kDataDir, ec)) {
            cerr << "Error: '" << kDataDir << "' exists but is not a directory." << endl;
            return false;
        }
        return true;
    }

    if (!std::filesystem::create_directories(kDataDir, ec)) {
        cerr << "Error creating data directory '" << kDataDir << "': " << ec.message() << endl;
        return false;
    }

    return true;
}

void User::syncCounterFromUsers(const vector<User>& loadedUsers) {
    const string todayPrefix = "0000" + getCurrentDate();
    int maxCounter = 0;

    for (const auto& user : loadedUsers) {
        const string& accountNumber = user.account_number;
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

bool User::parseTypeFromString(const string& typeText, Type& parsedType) {
    string normalized;
    normalized.reserve(typeText.size());
    for (char ch : typeText) {
        normalized.push_back(static_cast<char>(tolower(static_cast<unsigned char>(ch))));
    }

    if (normalized == "savings") {
        parsedType = SAVINGS;
        return true;
    }
    if (normalized == "current") {
        parsedType = CURRENT;
        return true;
    }

    return false;
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
}

// Destructor
User::~User() {
    // Empty Deconstructor
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

    if (account_type == CURRENT) {
        account_balance += amount;
        cout << "Credited $" << fixed << setprecision(2) << amount << " successfully." << endl;
        cout << "Updated Balance: $" << fixed << setprecision(2) << account_balance << endl;
        return;
    }

    const int64_t nowEpochSeconds = getCurrentEpochSeconds();
    pruneExpiredTransactions(nowEpochSeconds);

    if (!canApplyTransaction(amount, nowEpochSeconds)) {
        const double remaining = max(0.0, kDailyTransactionLimit - rolling24hVolume(nowEpochSeconds));
        cout << "Transaction denied: 24-hour volume limit exceeded." << endl;
        cout << "Remaining 24-hour volume: $" << fixed << setprecision(2) << remaining << endl;
        return;
    }

    account_balance += amount;
    recordTransaction(amount, nowEpochSeconds);
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

    if (account_type == CURRENT) {
        account_balance -= amount;
        cout << "Debited $" << fixed << setprecision(2) << amount << " successfully." << endl;
        cout << "Updated Balance: $" << fixed << setprecision(2) << account_balance << endl;
        return true;
    }

    const int64_t nowEpochSeconds = getCurrentEpochSeconds();
    pruneExpiredTransactions(nowEpochSeconds);

    if (!canApplyTransaction(-amount, nowEpochSeconds)) {
        const double remaining = max(0.0, kDailyTransactionLimit - rolling24hVolume(nowEpochSeconds));
        cout << "Transaction denied: 24-hour volume limit exceeded." << endl;
        cout << "Remaining 24-hour volume: $" << fixed << setprecision(2) << remaining << endl;
        return false;
    }

    account_balance -= amount;
    recordTransaction(-amount, nowEpochSeconds);
    cout << "Debited $" << fixed << setprecision(2) << amount << " successfully." << endl;
    cout << "Updated Balance: $" << fixed << setprecision(2) << account_balance << endl;
    return true;
}

// Modify existing account
void User::modifyAccount() {
    const string oldTransactionsPath = transactionFilePath();

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

        const string newTransactionsPath = transactionFilePath();
        if (newTransactionsPath != oldTransactionsPath) {
            std::error_code ec;
            if (std::filesystem::exists(oldTransactionsPath, ec)) {
                std::filesystem::rename(oldTransactionsPath, newTransactionsPath, ec);
                if (ec) {
                    cerr << "Warning: Could not rename transaction history file for updated username: "
                         << ec.message() << endl;
                }
            }
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
                // Intentional behavior: account-type transitions reset the sliding-volume state.
                recent_transactions.clear();
                cout << "Account type updated successfully. Next change allowed after 24 hours." << endl;
            }
        }
    }
}

bool User::saveToCsv(const vector<User>& users) {
    if (!ensureDataDirectory()) {
        return false;
    }

    ofstream outFile(kCsvPath, ios::trunc);
    if (!outFile.is_open()) {
        perror("Error opening data/accounts.csv");
        cerr << "Error exporting accounts to CSV!" << endl;
        return false;
    }

    outFile << "Account Number,Name,Balance,Type," << kPasswordHeader << ","
            << kTypeChangeEpochHeader << "\n";
    outFile << fixed << setprecision(2);
    for (const auto& user : users) {
        outFile << escapeCsvField(user.account_number) << ","
                << escapeCsvField(user.user_name) << ","
                << user.account_balance << ","
                << escapeCsvField(accountTypeMap.at(user.account_type)) << ","
                << escapeCsvField(user.password_hash) << ","
                << user.last_account_type_change_epoch_seconds << "\n";
    }

    if (!outFile.good()) {
        perror("Error writing data/accounts.csv");
        cerr << "Error: Failed while writing CSV file." << endl;
        return false;
    }

    return true;
}

vector<User> User::loadFromCsv() {
    vector<User> loadedUsers;

    if (!ensureDataDirectory()) {
        return loadedUsers;
    }

    ifstream inFile(kCsvPath);
    if (!inFile.is_open()) {
        if (errno == ENOENT) {
            if (!persist(loadedUsers)) {
                cerr << "Error: Failed to initialize storage files in data/." << endl;
            }
            return loadedUsers;
        }

        perror("Error opening data/accounts.csv");
        cerr << "Error: Could not open CSV file for reading." << endl;
        return loadedUsers;
    }

    string line;
    size_t lineNumber = 0;
    bool headerConsumed = false;

    while (getline(inFile, line)) {
        ++lineNumber;
        if (line.empty()) {
            continue;
        }

        if (!headerConsumed) {
            headerConsumed = true;
            if (line.rfind("Account Number", 0) == 0) {
                continue;
            }
        }

        vector<string> fields = splitCsvLine(line);
        if (fields.size() != 4 && fields.size() != 5 && fields.size() != 6) {
            cerr << "Warning: Skipping malformed CSV row at line " << lineNumber << "." << endl;
            continue;
        }

        User user;
        user.account_number = fields[0];
        user.user_name = fields[1];

        try {
            user.account_balance = stod(fields[2]);
        } catch (const exception&) {
            cerr << "Warning: Invalid balance in CSV row at line " << lineNumber << "." << endl;
            continue;
        }

        Type parsedType = SAVINGS;
        if (!parseTypeFromString(fields[3], parsedType)) {
            cerr << "Warning: Invalid account type in CSV row at line " << lineNumber << "." << endl;
            continue;
        }
        user.account_type = parsedType;

        if (fields.size() >= 5) {
            user.password_hash = fields[4];
        } else {
            user.password_hash = "";
        }

        user.last_account_type_change_epoch_seconds = -1;
        if (fields.size() >= 6) {
            try {
                user.last_account_type_change_epoch_seconds = stoll(fields[5]);
            } catch (const exception&) {
                cerr << "Warning: Invalid type-change timestamp in CSV row at line " << lineNumber << "." << endl;
                user.last_account_type_change_epoch_seconds = -1;
            }
        }

        if (user.account_type == SAVINGS) {
            if (!loadTransactionsForUser(user)) {
                cerr << "Warning: Could not load transaction history for user " << user.user_name << "." << endl;
            }
        } else {
            user.recent_transactions.clear();
        }

        loadedUsers.push_back(user);
    }

    syncCounterFromUsers(loadedUsers);
    return loadedUsers;
}

bool User::persist(const vector<User>& users) {
    bool csvOk = saveToCsv(users);
    bool transactionsOk = saveTransactionsForUsers(users);
    return csvOk && transactionsOk;
}

bool User::exportToCSV(const vector<User>& users) {
    bool ok = persist(users);
    if (ok) {
        cout << "Accounts exported successfully to CSV." << endl;
    }
    return ok;
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
    if (account_type != SAVINGS) {
        return 0.0;
    }
    return rolling24hVolume(getCurrentEpochSeconds());
}

double User::getRemaining24hVolume() const {
    if (account_type != SAVINGS) {
        return 0.0;
    }
    return max(0.0, kDailyTransactionLimit - getCurrent24hVolume());
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
        recent_transactions.clear();
    }
    this->account_type = type;
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



