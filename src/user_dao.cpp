#include "user_dao.h"
#include "util.h"
#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>

using namespace std;

namespace {
const char* kPasswordHeader = "PasswordHash";
const char* kTypeChangeEpochHeader = "LastTypeChangeEpochSeconds";
const char* kTransactionHeader = "SerialNumber,EpochSeconds,Amount";
const char* kTransactionsSuffix = "_transactions.csv";
}

UserDAO::UserDAO(const string& dataDirectory)
    : data_directory(dataDirectory),
      transactions_directory(dataDirectory + "/transactions"),
      accounts_csv_path(dataDirectory + "/accounts.csv") {
}

bool UserDAO::ensureDataDirectory() const {
    std::error_code ec;
    if (std::filesystem::exists(data_directory, ec)) {
        if (!std::filesystem::is_directory(data_directory, ec)) {
            cerr << "Error: '" << data_directory << "' exists but is not a directory." << endl;
            return false;
        }
        return true;
    }

    if (!std::filesystem::create_directories(data_directory, ec)) {
        cerr << "Error creating data directory '" << data_directory << "': " << ec.message() << endl;
        return false;
    }

    return true;
}

bool UserDAO::parseTypeFromString(const string& typeText, Type& parsedType) {
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

string UserDAO::sanitizeUserNameForFile(const string& userName) {
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

string UserDAO::transactionFilePathForUserName(const string& userName) const {
    return transactions_directory + "/" + sanitizeUserNameForFile(userName) + kTransactionsSuffix;
}

bool UserDAO::loadTransactionsForRecord(UserRecord& record) const {
    record.recent_transactions.clear();

    ifstream inFile(transactionFilePathForUserName(record.user_name));
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
                 << record.user_name << " at line " << lineNumber << "." << endl;
            continue;
        }

        try {
            TransactionRecord transaction;
            transaction.epoch_seconds = stoll(fields[1]);
            transaction.amount = stod(fields[2]);
            record.recent_transactions.push_back(transaction);
        } catch (const exception&) {
            cerr << "Warning: Skipping invalid transaction row for user "
                 << record.user_name << " at line " << lineNumber << "." << endl;
        }
    }

    return true;
}

bool UserDAO::saveTransactionsForRecord(const UserRecord& record) const {
    if (!ensureDataDirectory()) {
        return false;
    }

    std::error_code ec;
    std::filesystem::create_directories(transactions_directory, ec);
    if (ec) {
        cerr << "Error: Could not create transactions directory: " << ec.message() << endl;
        return false;
    }

    ofstream outFile(transactionFilePathForUserName(record.user_name), ios::trunc);
    if (!outFile.is_open()) {
        perror("Error opening per-user transactions CSV");
        cerr << "Error: Could not write transactions for user " << record.user_name << "." << endl;
        return false;
    }

    outFile << kTransactionHeader << "\n";
    for (size_t i = 0; i < record.recent_transactions.size(); ++i) {
        const auto& transaction = record.recent_transactions[i];
        outFile << (i + 1) << "," << transaction.epoch_seconds << "," << transaction.amount << "\n";
    }

    if (!outFile.good()) {
        perror("Error writing per-user transactions CSV");
        cerr << "Error: Failed while writing transactions for user " << record.user_name << "." << endl;
        return false;
    }

    return true;
}

bool UserDAO::saveTransactionsForRecords(const vector<UserRecord>& records) const {
    bool allOk = true;
    for (const auto& record : records) {
        if (record.account_type == SAVINGS) {
            if (!saveTransactionsForRecord(record)) {
                allOk = false;
            }
        } else {
            std::error_code ec;
            std::filesystem::remove(transactionFilePathForUserName(record.user_name), ec);
        }
    }
    return allOk;
}

bool UserDAO::saveAccountsCsv(const vector<UserRecord>& records) const {
    if (!ensureDataDirectory()) {
        return false;
    }

    ofstream outFile(accounts_csv_path, ios::trunc);
    if (!outFile.is_open()) {
        perror("Error opening accounts CSV");
        cerr << "Error exporting accounts to CSV!" << endl;
        return false;
    }

    outFile << "Account Number,Name,Balance,Type," << kPasswordHeader << ","
            << kTypeChangeEpochHeader << "\n";
    outFile << fixed << setprecision(2);
    for (const auto& record : records) {
        outFile << escapeCsvField(record.account_number) << ","
                << escapeCsvField(record.user_name) << ","
                << record.account_balance << ","
                << escapeCsvField(accountTypeMap.at(record.account_type)) << ","
                << escapeCsvField(record.password_hash) << ","
                << record.last_account_type_change_epoch_seconds << "\n";
    }

    if (!outFile.good()) {
        perror("Error writing accounts CSV");
        cerr << "Error: Failed while writing CSV file." << endl;
        return false;
    }

    return true;
}

vector<UserRecord> UserDAO::loadRecords() const {
    vector<UserRecord> records;

    if (!ensureDataDirectory()) {
        return records;
    }

    ifstream inFile(accounts_csv_path);
    if (!inFile.is_open()) {
        if (errno == ENOENT) {
            if (!persistAll(vector<User>{})) {
                cerr << "Error: Failed to initialize storage files in data/." << endl;
            }
            return records;
        }

        perror("Error opening accounts CSV");
        cerr << "Error: Could not open CSV file for reading." << endl;
        return records;
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

        UserRecord record;
        record.account_number = fields[0];
        record.user_name = fields[1];

        try {
            record.account_balance = stod(fields[2]);
        } catch (const exception&) {
            cerr << "Warning: Invalid balance in CSV row at line " << lineNumber << "." << endl;
            continue;
        }

        Type parsedType = SAVINGS;
        if (!parseTypeFromString(fields[3], parsedType)) {
            cerr << "Warning: Invalid account type in CSV row at line " << lineNumber << "." << endl;
            continue;
        }
        record.account_type = parsedType;

        if (fields.size() >= 5) {
            record.password_hash = fields[4];
        } else {
            record.password_hash = "";
        }

        record.last_account_type_change_epoch_seconds = -1;
        if (fields.size() >= 6) {
            try {
                record.last_account_type_change_epoch_seconds = stoll(fields[5]);
            } catch (const exception&) {
                cerr << "Warning: Invalid type-change timestamp in CSV row at line " << lineNumber << "." << endl;
                record.last_account_type_change_epoch_seconds = -1;
            }
        }

        if (record.account_type == SAVINGS) {
            if (!loadTransactionsForRecord(record)) {
                cerr << "Warning: Could not load transaction history for user " << record.user_name << "." << endl;
            }
        } else {
            record.recent_transactions.clear();
        }

        records.push_back(record);
    }

    return records;
}

vector<User> UserDAO::loadAll() const {
    vector<UserRecord> records = loadRecords();
    vector<User> users;
    users.reserve(records.size());

    for (const auto& record : records) {
        users.push_back(User::fromRecord(record));
    }

    User::syncAccountCounterFromRecords(records);
    return users;
}

bool UserDAO::persistAll(const vector<User>& users) const {
    vector<UserRecord> records;
    records.reserve(users.size());
    for (const auto& user : users) {
        records.push_back(user.toRecord());
    }

    bool csvOk = saveAccountsCsv(records);
    bool transactionsOk = saveTransactionsForRecords(records);
    return csvOk && transactionsOk;
}

bool UserDAO::migrateTransactionFileForUserNameChange(const string& oldUserName,
                                                      const string& newUserName) const {
    if (oldUserName == newUserName) {
        return true;
    }

    const string oldPath = transactionFilePathForUserName(oldUserName);
    const string newPath = transactionFilePathForUserName(newUserName);
    if (oldPath == newPath) {
        return true;
    }

    std::error_code ec;
    if (!std::filesystem::exists(oldPath, ec)) {
        return true;
    }

    if (!ensureDataDirectory()) {
        return false;
    }

    std::filesystem::create_directories(transactions_directory, ec);
    if (ec) {
        cerr << "Warning: Could not create transactions directory for username migration: "
             << ec.message() << endl;
        return false;
    }

    std::filesystem::rename(oldPath, newPath, ec);
    if (ec) {
        cerr << "Warning: Could not rename transaction history file for updated username: "
             << ec.message() << endl;
        return false;
    }

    return true;
}
