#pragma once

#include "user.h"
#include <string>
#include <vector>

class UserDAO {
private:
    std::string data_directory;
    std::string transactions_directory;
    std::string accounts_csv_path;

    bool ensureDataDirectory() const;
    static bool parseTypeFromString(const std::string& typeText, Type& parsedType);
    static std::string sanitizeUserNameForFile(const std::string& userName);
    std::string transactionFilePathForUserName(const std::string& userName) const;
    bool loadTransactionsForRecord(UserRecord& record) const;
    bool saveTransactionsForRecord(const UserRecord& record) const;
    bool saveTransactionsForRecords(const std::vector<UserRecord>& records) const;
    bool saveAccountsCsv(const std::vector<UserRecord>& records) const;
    std::vector<UserRecord> loadRecords() const;

public:
    explicit UserDAO(const std::string& dataDirectory = "data");

    std::vector<User> loadAll() const;
    bool persistAll(const std::vector<User>& users) const;
    bool migrateTransactionFileForUserNameChange(const std::string& oldUserName,
                                                 const std::string& newUserName) const;
};
