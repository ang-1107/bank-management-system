#include "user.h"
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <sstream>

using namespace std;

const unordered_map<Type, string> accountTypeMap = {
    {SAVINGS, "Savings"},
    {CURRENT, "Current"}
};

int User::counter = 0;                              // Initializing the static counter

namespace {
const char* kDataDir = "data";
const char* kCsvPath = "data/accounts.csv";
const char* kJsonPath = "data/accounts.json";

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
}  // namespace

// Generates current date in YYYYMMDD format
string User::getCurrentDate() {
    time_t now = time(0);
    char dateStr[9];
    strftime(dateStr, sizeof(dateStr), "%Y%m%d", localtime(&now));
    return string(dateStr);
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
    account_balance = 0.0;
    account_type = SAVINGS;
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
    if (amount > 0) {
        account_balance += amount;
        cout << "Credited $" << fixed << setprecision(2) << amount << " successfully." << endl;
        cout << "Updated Balance: $" << fixed << setprecision(2) << account_balance << endl;
    } else {
        cout << "Invalid amount!" << endl;
    }
}

// Withdraw money if there is sufficient balance
bool User::withdraw(double amount) {
    if (amount > 0 && amount <= account_balance) {
        account_balance -= amount;
        cout << "Debited $" << fixed << setprecision(2) << amount << " successfully." << endl;
        cout << "Updated Balance: $" << fixed << setprecision(2) << account_balance << endl;
        return true;
    } else {
        cout << "Insufficient balance or invalid amount!" << endl;
        return false;
    }
}

// Modify existing account
void User::modifyAccount() {
    cout << "Modify Account Details" << endl;
    cout << "Current Name: " << user_name << endl;
    cout << "Enter New Name: ";
    getline(cin >> ws, user_name);

    while (user_name.empty()) {
        cout << "Name cannot be empty. Enter New Name: ";
        getline(cin >> ws, user_name);
    }

    int accTypeInput;
    cout << "Current Account Type: " << accountTypeMap.at(account_type) << endl;
    cout << "Enter New Type (0 for Savings, 1 for Current): ";
    while (!(cin >> accTypeInput) || (accTypeInput != 0 && accTypeInput != 1)) {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "Invalid choice! Enter 0 for Savings or 1 for Current: ";
    }

    cin.ignore(numeric_limits<streamsize>::max(), '\n');

    account_type = static_cast<Type>(accTypeInput);
    cout << "Account Details Updated Successfully!" << endl;
}

// Converts User object to JSON format
json User::toJson() const {
    return json{
        {"account_number", account_number},
        {"user_name", user_name},
        {"account_balance", account_balance},
        {"account_type", accountTypeMap.at(account_type)}
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
            user.account_balance = jUser.value("account_balance", 0.0);

            Type parsed = SAVINGS;
            if (!parseTypeFromString(jUser.value("account_type", "Savings"), parsed)) {
                parsed = SAVINGS;
            }
            user.account_type = parsed;

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

    outFile << "Account Number,Name,Balance,Type\n";
    outFile << fixed << setprecision(2);
    for (const auto& user : users) {
        outFile << escapeCsvField(user.account_number) << ","
                << escapeCsvField(user.user_name) << ","
                << user.account_balance << ","
                << escapeCsvField(accountTypeMap.at(user.account_type)) << "\n";
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
        if (fields.size() != 4) {
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
    return csvOk && jsonOk;
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

Type User::getAccountType() const {
    return account_type;
}

// Get-Set Methods for Testing
void User::setUserName(const std::string& name) {
    this->user_name = name;
    return;
}

void User::setAccountType(Type type) {
    this->account_type = type;
    return;
}

void User::setBalance(double balance) {
    this->account_balance = balance;
    return;
}

double User::getBalance() const {
    return this->account_balance;
}



