# Bank Management System

![Made with C++](https://img.shields.io/badge/Made%20with-C%2B%2B-00599C?style=for-the-badge&logo=c%2B%2B&logoColor=white)
![MIT License](https://img.shields.io/badge/License-MIT-yellow.svg?style=for-the-badge)
![Testing: Catch2](https://img.shields.io/badge/Testing-Catch2-25c2a0?style=for-the-badge)
![JSON: nlohmann/json](https://img.shields.io/badge/JSON-nlohmann%2Fjson-1f8ceb?style=for-the-badge)

A lightweight C++ console banking application with account creation, authentication, account updates, transactions, and robust persistence.

## Features

- Create accounts (Savings and Current)
- Login with account number and password (SHA-256 hash verification)
- View account details
- Modify account name, account type, or both (account type change cooldown: once per 24 hours)
- Deposit and withdraw money
- CSV-first persistence on startup
- Automatic JSON synchronization from CSV state
- JSON fallback when CSV is unavailable
- Rolling 24-hour transaction volume limit (`<= 100000`) for Savings accounts only
- Per-user transaction history files with serial number, epoch timestamp, and amount (Savings accounts only)
- Signal handling for graceful exit message on SIGINT/SIGTERM
- Integration and regression test coverage

## Table of Contents

1. [Installation and Setup](#installation-and-setup)
2. [Usage Guide](#usage-guide)
3. [Architecture and Persistence Model](#architecture-and-persistence-model)
4. [Code Explanation](#code-explanation)
5. [Data Storage Formats](#data-storage-formats)
6. [File and Directory Structure](#file-and-directory-structure)
7. [Test Suite](#test-suite)
8. [Error Handling](#error-handling)
9. [Acknowledgment and Citation](#acknowledgment-and-citation)
10. [Contribution Guidelines](#contribution-guidelines)

## Installation and Setup

### Prerequisites

- C++ compiler (GCC, Clang, MSVC, etc.)
- C++17 or later
- Bundled nlohmann/json header in include/json.hpp

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
- State is persisted to CSV and synchronized to JSON.

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
- Startup source of truth:
  - Accounts are loaded from `data/accounts.csv` at program start.
- Fallback behavior:
  - If CSV is unavailable and JSON exists, JSON is loaded and CSV is initialized from JSON.
- Disk persistence strategy:
  - Any mutating operation triggers `persist(users)`.
  - `persist(users)` writes `data/accounts.csv` and `data/accounts.json` from the same vector.
  - `persist(users)` also writes per-user transaction files in `data/`.
- Directory handling:
  - The `data` directory is created automatically if it does not exist.
- Sliding-window enforcement:
  - Savings transactions compute rolling 24-hour volume using epoch timestamps.
  - If `current_volume + abs(new_amount) > 100000`, the Savings transaction is rejected.
  - Current transactions bypass this check and do not maintain rolling-volume history.
  - Changing account type resets rolling-volume state.
- Account type change cooldown:
  - A user can change account type only once every 24 hours.
  - Last type-change time is stored per user in account persistence data.

## Code Explanation

### `User` Class

Core data members:

- account_number
- user_name
- password_hash
- account_balance
- account_type

Main behaviors:

- `createAccount()`: reads name, type, password, and initializes a new account.
- `displayAccount()`: prints account details.
- `modifyAccount()`: updates name, type, or both based on user choice.
- `deposit(double amount)`: credits money to account balance.
- `withdraw(double amount)`: debits money if sufficient balance exists.
- `toJson()`: serializes a `User` object to JSON.
- `setPassword(const std::string&)`: hashes and stores password with SHA-256.
- `verifyPassword(const std::string&)`: verifies password by hash comparison.
- `isPasswordPolicyValid(const std::string&)`: validates printable ASCII password policy.
- `getCurrent24hVolume()`: returns current rolling 24-hour transaction volume for Savings accounts.
- `getRemaining24hVolume()`: returns remaining allowable volume in the window for Savings accounts.

Persistence and synchronization helpers:

- `loadFromCsv()`: startup read path and source of truth.
- `saveToCsv(const std::vector<User>&)`: writes canonical CSV data.
- `loadFromJson()`: JSON read helper used for verification/sync checks.
- `saveToJson(const std::vector<User>&)`: writes synchronized JSON snapshot.
- `persist(const std::vector<User>&)`: writes both CSV and JSON from the same vector.
- `exportToCSV(const std::vector<User>&)`: export entry point that keeps JSON in sync.

### Main Program Flow

- `main()` in `src/main.cpp` loads user state from CSV at startup.
- Top-level menu exposes `Create Account`, `Login`, and `Exit`.
- Successful authentication enters account session menu (`View`, `Modify`, `Deposit`, `Withdraw`, `Logout`).
- Any mutation (`create`, `modify`, `deposit`, `withdraw`) calls `persist(users)`.
- `SIGINT` and `SIGTERM` print the same exit message as regular `Exit`.

## Data Storage Formats

The application stores records in both CSV and JSON. CSV is read at startup, and JSON is maintained as a synchronized mirror.

### CSV Format: data/accounts.csv

```csv
Account Number,Name,Balance,Type,PasswordHash
0000202604181,John Doe,1000.50,Savings,8f7d6f6a6a8f1a7c9b8c4f4b4c7e8a5d23f7f7d5c9a0b8f6a2d1b4e6c8f3a2b1
0000202604182,Jane Smith,250.00,Current,41c4d5ea2f70bbf6296e92fcb8f9f7c4ed5f91c4ae9d8b6a9e4f6c2b8d7a0f13
```

### Per-user Transaction History Format: data/<user_name>_transactions.csv

This file is maintained for Savings accounts only.

```csv
SerialNumber,EpochSeconds,Amount
1,1713456000,1200.00
2,1713459600,-300.00
```

`Amount` is signed (`+` deposit, `-` withdrawal). Rolling volume uses `abs(Amount)` over the latest 24 hours.

### JSON Format: data/accounts.json

```json
[
  {
    "account_number": "0000202604181",
    "user_name": "John Doe",
    "password_hash": "8f7d6f6a6a8f1a7c9b8c4f4b4c7e8a5d23f7f7d5c9a0b8f6a2d1b4e6c8f3a2b1",
    "account_balance": 1000.5,
    "account_type": "Savings"
  },
  {
    "account_number": "0000202604182",
    "user_name": "Jane Smith",
    "password_hash": "41c4d5ea2f70bbf6296e92fcb8f9f7c4ed5f91c4ae9d8b6a9e4f6c2b8d7a0f13",
    "account_balance": 250.0,
    "account_type": "Current"
  }
]
```

## File and Directory Structure

```text
bank-management-system/
├── .gitignore
├── README.md
├── Makefile
├── data/
│   ├── accounts.csv
│   └── accounts.json
├── include/
│   ├── json.hpp
│   └── user.h
├── src/
│   ├── main.cpp
│   └── user.cpp
├── tests/
│   ├── catch.hpp
│   └── user_test.cpp
└── LICENSE
```

## Test Suite

The project includes unit, integration, and regression tests using Catch2.

Latest verified run: 28 test cases and 119 assertions.

### Coverage Highlights

- Unit tests:
  - Deposit behavior
  - Withdraw success and failure behavior
  - Account type serialization
  - Password policy and hash determinism
  - Modify account behavior (name-only, type-only, both)
  - 24-hour volume boundary behavior
  - Current-account bypass of 24-hour volume checks
  - Account-type switch reset behavior for rolling volume
- Integration tests:
  - Storage initialization creates data directory and files
  - CSV startup loading with JSON synchronization
  - JSON fallback when CSV is missing
  - CSV precedence when both CSV and JSON exist
  - CSV parsing with quoted fields
  - Per-user Savings transactions file format and serial numbering
  - Current-account transaction file absence
- Regression tests:
  - Persisted updates remain consistent after reload from CSV and JSON
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
- JSON parse failures are reported with error details

## Acknowledgment and Citation

This project uses the nlohmann/json library for JSON parsing and serialization.

Lohmann, N. (2025). JSON for Modern C++ (Version 3.12.0) [Computer software]. https://github.com/nlohmann/json

## Contribution Guidelines

- Fork the repository.
- Make changes in a separate branch.
- Submit a pull request with a clear description of your updates.


