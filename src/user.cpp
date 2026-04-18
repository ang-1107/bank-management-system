#include "user.h"
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
const char* kJsonPath = "data/accounts.json";
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

inline uint32_t rotr(uint32_t value, uint32_t shift) {
    return (value >> shift) | (value << (32U - shift));
}

string sha256(const string& input) {
    // SHA-256 round constants from FIPS 180-4 (part of the standard algorithm).
    static constexpr uint32_t k[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
        0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
        0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
        0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
        0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
        0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
        0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
        0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
        0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
    };

    array<uint32_t, 8> h = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };

    vector<uint8_t> data(input.begin(), input.end());
    uint64_t bitLength = static_cast<uint64_t>(data.size()) * 8ULL;

    data.push_back(0x80);
    while ((data.size() % 64) != 56) {
        data.push_back(0x00);
    }

    for (int i = 7; i >= 0; --i) {
        data.push_back(static_cast<uint8_t>((bitLength >> (8 * i)) & 0xFF));
    }

    for (size_t chunk = 0; chunk < data.size(); chunk += 64) {
        uint32_t w[64] = {0};

        for (int i = 0; i < 16; ++i) {
            const size_t offset = chunk + static_cast<size_t>(i) * 4;
            w[i] = (static_cast<uint32_t>(data[offset]) << 24) |
                   (static_cast<uint32_t>(data[offset + 1]) << 16) |
                   (static_cast<uint32_t>(data[offset + 2]) << 8) |
                    static_cast<uint32_t>(data[offset + 3]);
        }

        for (int i = 16; i < 64; ++i) {
            const uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            const uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }

        uint32_t a = h[0];
        uint32_t b = h[1];
        uint32_t c = h[2];
        uint32_t d = h[3];
        uint32_t e = h[4];
        uint32_t f = h[5];
        uint32_t g = h[6];
        uint32_t hh = h[7];

        for (int i = 0; i < 64; ++i) {
            const uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            const uint32_t ch = (e & f) ^ ((~e) & g);
            const uint32_t temp1 = hh + s1 + ch + k[i] + w[i];
            const uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            const uint32_t temp2 = s0 + maj;

            hh = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }

        h[0] += a;
        h[1] += b;
        h[2] += c;
        h[3] += d;
        h[4] += e;
        h[5] += f;
        h[6] += g;
        h[7] += hh;
    }

    stringstream ss;
    ss << hex << setfill('0');
    for (uint32_t value : h) {
        ss << setw(8) << value;
    }
    return ss.str();
}

string escapeCsvField(const string& field) {
    bool needsQuotes = (field.find(',') != string::npos) || (field.find('"') != string::npos);
    if (!needsQuotes) {
        return field;
    }

    string escaped;
    escaped.reserve(field.size() + 2);
    escaped.push_back('"');
    for (char ch : field) {
        if (ch == '"') {
            escaped.push_back('"');
        }
        escaped.push_back(ch);
    }
    escaped.push_back('"');
    return escaped;
}

vector<string> splitCsvLine(const string& line) {
    vector<string> fields;
    string current;
    bool inQuotes = false;

    for (size_t i = 0; i < line.size(); ++i) {
        char ch = line[i];
        if (ch == '"') {
            if (inQuotes && i + 1 < line.size() && line[i + 1] == '"') {
                current.push_back('"');
                ++i;
            } else {
                inQuotes = !inQuotes;
            }
        } else if (ch == ',' && !inQuotes) {
            fields.push_back(current);
            current.clear();
        } else {
            current.push_back(ch);
        }
    }

    fields.push_back(current);
    return fields;
}

string formatCooldownTime(int64_t remainingSeconds) {
    if (remainingSeconds <= 0) {
        return "0m";
    }

    int64_t hours = remainingSeconds / 3600;
    int64_t minutes = (remainingSeconds % 3600 + 59) / 60;
    if (minutes == 60) {
        ++hours;
        minutes = 0;
    }

    stringstream ss;
    if (hours > 0) {
        ss << hours << "h";
    }
    if (minutes > 0 || hours == 0) {
        if (hours > 0) {
            ss << " ";
        }
        ss << minutes << "m";
    }

    return ss.str();
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

// Converts User object to JSON format
json User::toJson() const {
    json txnArray = json::array();
    for (const auto& record : recent_transactions) {
        txnArray.push_back({
            {"epoch_seconds", record.epoch_seconds},
            {"amount", record.amount}
        });
    }

    return json{
        {"account_number", account_number},
        {"user_name", user_name},
        {"password_hash", password_hash},
        {"account_balance", account_balance},
        {"account_type", accountTypeMap.at(account_type)},
        {"last_account_type_change_epoch_seconds", last_account_type_change_epoch_seconds},
        {"recent_transactions", txnArray}
    };
}

// Saves all accounts to a JSON file
bool User::saveToJson(const vector<User>& users) {
    if (!ensureDataDirectory()) {
        return false;
    }

    json jArray = json::array();
    for (const auto& user : users) {
        jArray.push_back(user.toJson());
    }

    ofstream outFile(kJsonPath, ios::trunc);
    if (!outFile.is_open()) {
        perror("Error opening data/accounts.json");
        cerr << "Error: Could not open JSON file for writing!" << endl;
        return false;
    }

    outFile << jArray.dump(4);
    if (!outFile.good()) {
        perror("Error writing data/accounts.json");
        cerr << "Error: Failed while writing JSON file." << endl;
        return false;
    }

    return true;
}

// Loads all accounts from a JSON file
vector<User> User::loadFromJson() {
    vector<User> loadedUsers;

    if (!ensureDataDirectory()) {
        return loadedUsers;
    }

    ifstream inFile(kJsonPath);
    if (!inFile.is_open()) {
        return loadedUsers;
    }

    try {
        json jArray;
        inFile >> jArray;

        for (const auto& jUser : jArray) {
            User user;
            user.account_number = jUser.value("account_number", "");
            user.user_name = jUser.value("user_name", "");
            user.password_hash = jUser.value("password_hash", "");
            user.account_balance = jUser.value("account_balance", 0.0);

            Type parsed = SAVINGS;
            if (!parseTypeFromString(jUser.value("account_type", "Savings"), parsed)) {
                parsed = SAVINGS;
            }
            user.account_type = parsed;
            user.last_account_type_change_epoch_seconds =
                jUser.value("last_account_type_change_epoch_seconds", static_cast<int64_t>(-1));

            if (user.account_type != SAVINGS) {
                user.recent_transactions.clear();
            }

            json txnArray = jUser.value("recent_transactions", json::array());
            if (user.account_type == SAVINGS && txnArray.is_array()) {
                for (const auto& txn : txnArray) {
                    user.recent_transactions.push_back(TransactionRecord{
                        txn.value("epoch_seconds", static_cast<int64_t>(0)),
                        txn.value("amount", 0.0)
                    });
                }
            }

            loadedUsers.push_back(user);
        }
    } catch (const exception& ex) {
        cerr << "Error: Failed to parse JSON file data/accounts.json: " << ex.what() << endl;
        return {};
    }

    syncCounterFromUsers(loadedUsers);
    return loadedUsers;
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
            std::error_code ec;
            if (std::filesystem::exists(kJsonPath, ec)) {
                loadedUsers = loadFromJson();
                if (!saveToCsv(loadedUsers)) {
                    cerr << "Error: Failed to initialize CSV from JSON fallback data." << endl;
                }
                if (!saveTransactionsForUsers(loadedUsers)) {
                    cerr << "Error: Failed to initialize transaction files from JSON fallback data." << endl;
                }
                return loadedUsers;
            }

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

    if (!saveToJson(loadedUsers)) {
        cerr << "Warning: Failed to synchronize JSON after reading CSV." << endl;
    }

    return loadedUsers;
}

bool User::persist(const vector<User>& users) {
    bool csvOk = saveToCsv(users);
    bool jsonOk = saveToJson(users);
    bool transactionsOk = saveTransactionsForUsers(users);
    return csvOk && jsonOk && transactionsOk;
}

bool User::exportToCSV(const vector<User>& users) {
    bool ok = persist(users);
    if (ok) {
        cout << "Accounts exported successfully to CSV and synchronized to JSON." << endl;
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



