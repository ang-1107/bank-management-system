#pragma once            // To ensure that this file is included only once
#include <iostream>
#include <fstream>      // For file handling operations
#include <iomanip>      // For formatted output
#include <cctype>       // For character operations
#include <string>       // For string operations
#include <unordered_map>// For unordered map
#include <ctime>        // For date-based account numbers
#include <vector>       // For storing multiple user objects
#include <cstddef>      // For size_t
#include <cstdint>      // For fixed width integer types
#include "json.hpp"     // JSON library for serialization

using json = nlohmann::json;    // Alias for easier usage

// Enum for account types
typedef enum Type_ { SAVINGS = 0, CURRENT = 1 } Type;
extern const std::unordered_map<Type, std::string> accountTypeMap;  // Maps Type to string for scalability

struct TransactionRecord {
    int64_t epoch_seconds;
    double amount;
};

class User {
private:
    std::string account_number;
    std::string user_name;
    std::string password_hash;
    double account_balance;
    Type account_type;
    int64_t last_account_type_change_epoch_seconds;
    std::vector<TransactionRecord> recent_transactions;

    static int counter;
    static int64_t time_override_epoch_seconds;
    static std::string getCurrentDate();    // Static function to get formatted date
    static int64_t getCurrentEpochSeconds();
    static bool ensureDataDirectory();
    static void syncCounterFromUsers(const std::vector<User>& loadedUsers);
    static bool parseTypeFromString(const std::string& typeText, Type& parsedType);
    static std::string sanitizeUserNameForFile(const std::string& userName);
    std::string transactionFilePath() const;
    static bool loadTransactionsForUser(User& user);
    static bool saveTransactionsForUser(const User& user);
    static bool saveTransactionsForUsers(const std::vector<User>& users);
    void pruneExpiredTransactions(int64_t nowEpochSeconds);
    double rolling24hVolume(int64_t nowEpochSeconds) const;
    bool canApplyTransaction(double signedAmount, int64_t nowEpochSeconds) const;
    void recordTransaction(double signedAmount, int64_t nowEpochSeconds);

public:
    User();                     // Constructor
    ~User();                    // Destructor

    void createAccount();        // Creates a new account
    void displayAccount() const; // Displays account details
    void modifyAccount();        // Modifies account (name & type)
    void deposit(double amount); // Deposits money
    bool withdraw(double amount);// Withdraws money

    // Getter for account_number
    std::string getAccountNumber() const; 
    std::string getUserName() const;
    std::string getPasswordHash() const;
    Type getAccountType() const;
    bool verifyPassword(const std::string& plainPassword) const;
    void setPassword(const std::string& plainPassword);
    static bool isPasswordPolicyValid(const std::string& plainPassword);
    double getCurrent24hVolume() const;
    double getRemaining24hVolume() const;
    static void setTimeOverrideForTesting(int64_t epochSeconds);
    static void clearTimeOverrideForTesting();

    // JSON Serialization
    json toJson() const;
    static bool saveToJson(const std::vector<User>& users);
    static std::vector<User> loadFromJson();

    // CSV Storage
    static bool saveToCsv(const std::vector<User>& users);
    static std::vector<User> loadFromCsv();

    // Write both CSV and JSON from the same source vector
    static bool persist(const std::vector<User>& users);

    // CSV Export alias
    static bool exportToCSV(const std::vector<User>& users);

    // Get-Set Methods for Testing
    void setUserName(const std::string& name);
    void setAccountType(Type type);
    void setPasswordHash(const std::string& hashValue);
    void setBalance(double balance);
    double getBalance() const;
};

