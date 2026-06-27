# Bank Management System

![Made with C++](https://img.shields.io/badge/Made%20with-C%2B%2B-00599C?style=for-the-badge&logo=c%2B%2B&logoColor=white)
![Testing: Catch2](https://img.shields.io/badge/Testing-Catch2-25c2a0?style=for-the-badge)
![Design: OOP](https://img.shields.io/badge/Design-OOP-2E8B57?style=for-the-badge)
![MIT License](https://img.shields.io/badge/License-MIT-yellow.svg?style=for-the-badge)

A lightweight C++ console banking application with account creation, authentication, account updates, transactions, and robust persistence.

## Table of Contents

1. [Features](#features)
2. [Installation and Setup](#installation-and-setup)
3. [Usage Guide](#usage-guide)
4. [Architecture and Persistence Model](#architecture-and-persistence-model)
5. [Code Explanation](#code-explanation)
6. [Data Storage Formats](#data-storage-formats)
7. [File and Directory Structure](#file-and-directory-structure)
8. [Test Suite](#test-suite)
9. [Error Handling](#error-handling)
10. [Optional CSV to JSON Utility](#optional-csv-to-json-utility)
11. [Contribution Guidelines](#contribution-guidelines)

## Features

- Create accounts (Savings and Current)
- Login with account number and password (SHA-256 hash verification)
- View account details
- Modify account name, account type, or both (account type change cooldown: once per 24 hours)
- Deposit and withdraw money
- CSV-first persistence on startup
- Rolling 24-hour transaction volume limit (`<= 100000`) for Savings accounts only
- Per-user transaction history files with serial number, epoch timestamp, and amount (Savings accounts only)
- Signal handling for graceful exit message on SIGINT/SIGTERM
- Integration and regression test coverage

## Installation and Setup

### Prerequisites

- C++ compiler (GCC, Clang, MSVC, etc.)
- C++17 or later

### Build and Run

1. Clone the repository:

```sh
git clone https://github.com/ang-1107/bank-management-system.git
cd bank-management-system
```

2. Build:

```sh
make
```

3. Run:

```sh
make run
```

4. Run tests:

```sh
make test
```

5. Clean build artifacts:

```sh
make clean
```

## Usage Guide

### 1. Create Account

- Enter name, account type, and password (with confirmation).
- A date-based account number is generated.
- Password is stored as SHA-256 hash in persisted files.
- State is persisted to CSV.

### 2. Login

1. Enter account number.
2. Enter password.
3. On successful verification, enter the authenticated account menu.

### 3. Authenticated Account Menu

1. View Account
2. Modify Account
3. Deposit Money
4. Withdraw Money
5. Logout

When you choose `View Account` for a Savings account, the app displays:
- current rolling 24-hour transaction volume
- remaining allowable volume in the same 24-hour window

Because this is a sliding window, these values are recalculated from current time each time you run `View Account` and can change between consecutive checks.

For Current accounts, 24-hour volume is not enforced, not tracked, and not shown.

### 4. Modify Account

1. Choose what to update: name, type, or both.
2. Account type can be changed at most once in 24 hours (cooldown applies in both directions).
3. Submit updated value(s).
4. Changes are persisted immediately.

### 5. Deposit and Withdraw

1. Enter amount in authenticated session.
2. On valid transactions, changes are persisted to disk.

### 6. Logout and Exit

1. Logout returns to the top-level Create/Login/Exit menu.
2. Exit and SIGINT/SIGTERM print the same goodbye message.

## Architecture and Persistence Model

- Single source of runtime state:
  - `src/main.cpp` owns one `vector<User>` for consistent state management.
- DAO-backed persistence:
  - `UserDAO` loads accounts from `data/accounts.csv`.
  - `UserDAO` persists canonical account records to `data/accounts.csv`.
  - `UserDAO` persists Savings transaction history files in `data/transactions/`.
  - `UserRecord` is the data-transfer boundary between CSV storage and the `User` domain model.
- Directory handling:
  - The `data` directory is created automatically if it does not exist.
- Sliding-window enforcement:
  - Savings transactions compute rolling 24-hour volume using epoch timestamps.
  - If `current_volume + abs(requested_amount) > 100000`, the Savings transaction is rejected.
  - Current transactions bypass this check and do not maintain rolling-volume history.
  - Changing account type resets rolling-volume state.
- Account type change cooldown:
  - A user can change account type only once every 24 hours.
  - Last type-change time is stored per user in account persistence data.

## Code Explanation

### Account Behavior Pattern (Inheritance & Polymorphism)

The application uses a behavior-based design to separate account-type-specific logic from the core `User` class. This follows the Strategy and Polymorphism patterns.

**AccountBehavior Hierarchy:**
- `AccountBehavior`: abstract base class defining the interface for account-specific rules.
  - `SavingsBehavior`: enforces rolling 24-hour transaction volume limits and maintains transaction history.
  - `CurrentBehavior`: no volume enforcement; transactions are processed without restriction.

Each `User` instance holds a `unique_ptr<AccountBehavior>` that is selected at construction time based on account type. When a user performs a deposit or withdrawal, the operation is delegated to the behavior object, which encapsulates type-specific rules. If the account type changes, the behavior is swapped to match the selected type.

**Benefits:**
- Single Responsibility: account-type rules are isolated in behavior classes.
- Open/Closed Principle: adding another account type requires a behavior class without modifying `User`.
- Liskov Substitution: `SavingsBehavior` and `CurrentBehavior` are interchangeable implementations of the same contract.

### `User` Class

Core data members:

- account_number
- user_name
- password_hash
- account_balance
- account_type

Main behaviors:

- `createAccount()`: reads name, type, password, and initializes an account.
- `displayAccount()`: prints account details.
- `modifyAccount()`: updates name, type, or both based on user choice.
- `deposit(double amount)`: credits money to account balance.
- `withdraw(double amount)`: debits money if sufficient balance exists.
- `setPassword(const std::string&)`: hashes and stores password with SHA-256.
- `verifyPassword(const std::string&)`: verifies password by hash comparison.
- `isPasswordPolicyValid(const std::string&)`: validates printable ASCII password policy.
- `getCurrent24hVolume()`: returns current rolling 24-hour transaction volume for Savings accounts.
- `getRemaining24hVolume()`: returns remaining allowable volume in the window for Savings accounts.
- `fromRecord(const UserRecord&)`: hydrates a domain object from DAO data.
- `toRecord()`: exports domain state for DAO persistence.

### User DAO

`UserDAO` owns persistence concerns and keeps file handling outside the domain model.

Main responsibilities:

- `loadAll()`: loads account records and returns hydrated `User` objects.
- `persistAll(const std::vector<User>&)`: writes account records and Savings transaction files.
- `migrateTransactionFileForUserNameChange(...)`: preserves username-based transaction file continuity when a user changes name.
- CSV parsing and serialization
- Transaction file path generation
- Storage directory creation
- Legacy CSV row compatibility

### Utility Module (`util`)

Provides reusable, domain-independent helper functions to reduce coupling and improve testability:

- **`sha256(const std::string&)`**: Computes SHA-256 hash for password storage and verification.
- **`splitCsvLine(const std::string&)`**: Parses CSV lines, correctly handling quoted fields with embedded commas and quotes.
- **`escapeCsvField(const std::string&)`**: Escapes CSV field values (wraps in quotes if needed, escapes internal quotes).
- **`formatCooldownTime(int64_t)`**: Converts epoch timestamps to human-readable duration format (e.g., "23h 45m 30s").

These utilities are independent of domain logic and can be extended, replaced, or reused in other modules without coupling to the `User` or account behavior logic.

### Main Program Flow

- `main()` in `src/main.cpp` loads user state through `UserDAO`.
- Top-level menu exposes `Create Account`, `Login`, and `Exit`.
- Successful authentication enters account session menu (`View`, `Modify`, `Deposit`, `Withdraw`, `Logout`).
- Any mutation (`create`, `modify`, `deposit`, `withdraw`) calls `UserDAO::persistAll(users)`.
- `SIGINT` and `SIGTERM` print the same exit message as regular `Exit`.

## Data Storage Formats

The application stores records in CSV files.

### CSV Format: data/accounts.csv

```csv
Account Number,Name,Balance,Type,PasswordHash
0000202604181,John Doe,1000.50,Savings,8f7d6f6a6a8f1a7c9b8c4f4b4c7e8a5d23f7f7d5c9a0b8f6a2d1b4e6c8f3a2b1
0000202604182,Jane Smith,250.00,Current,41c4d5ea2f70bbf6296e92fcb8f9f7c4ed5f91c4ae9d8b6a9e4f6c2b8d7a0f13
```

### Per-user Transaction History Format: data/transactions/<user_name>_transactions.csv

This file is maintained for Savings accounts only.

```csv
SerialNumber,EpochSeconds,Amount
1,1713456000,1200.00
2,1713459600,-300.00
```

`Amount` is signed (`+` deposit, `-` withdrawal). Rolling volume uses `abs(Amount)` over the latest 24 hours.

## File and Directory Structure

```text
bank-management-system/
├── .gitignore
├── README.md
├── Makefile
├── data/
│   ├── accounts.csv
│   └── transactions/
│       └── <user_name>_transactions.csv
├── include/
│   ├── account_behavior.h
│   ├── user.h
│   ├── user_dao.h
│   └── util.h
├── src/
│   ├── account_behavior.cpp
│   ├── main.cpp
│   ├── user.cpp
│   ├── user_dao.cpp
│   └── util.cpp
├── tests/
│   ├── catch.hpp
│   ├── test_helpers.h
│   ├── user_auth_test.cpp
│   ├── user_basic_test.cpp
│   ├── user_copy_move_test.cpp
│   ├── user_modify_test.cpp
│   ├── user_persistence_test.cpp
│   ├── user_test_main.cpp
│   └── user_volume_test.cpp
└── LICENSE
```

## Test Suite

The project includes unit, integration, and regression tests using Catch2.

Verified run: 35 test cases and 152 assertions.

### Coverage Highlights

- Unit tests:
  - Deposit behavior
  - Withdraw success and failure behavior
  - Account type set/get behavior
  - Password policy and hash determinism
  - Modify account behavior (name-only, type-only, both)
  - 24-hour volume boundary behavior
  - Current-account bypass of 24-hour volume checks
  - Account-type switch reset behavior for rolling volume
- Integration tests:
  - Storage initialization creates data directory and files
  - CSV startup loading and parsing
  - CSV parsing with quoted fields
  - Per-user Savings transactions file format and serial numbering
  - Current-account transaction file absence
- Regression tests:
  - Persisted updates remain consistent across CSV reload
  - Password hash persistence and verification across reload
  - Legacy CSV compatibility and stress scenarios
  - Reloaded rolling-volume correctness from persisted transaction history

Run the full test suite with:

```sh
make test
```

## Error Handling

- Input validation for menu choices and transaction amounts
- Password validation for printable ASCII characters excluding whitespace/control
- Insufficient balance checks for withdrawal
- File I/O diagnostics through stderr and perror for open/write/read failures
- Graceful handling of malformed CSV rows with warnings

## Optional CSV to JSON Utility

If you want JSON output for external tooling, use the helper script:

```sh
python scripts/csv_to_json.py data/accounts.csv data/accounts.json
```

This conversion is optional and does not affect the application's CSV-only runtime persistence.

## Contribution Guidelines

- Fork the repository.
- Make changes in a separate branch.
- Submit a pull request with a clear description of your updates.


