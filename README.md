# Bank Management System

![Made with C++](https://img.shields.io/badge/Made%20with-C%2B%2B-00599C?style=for-the-badge&logo=c%2B%2B&logoColor=white)
![MIT License](https://img.shields.io/badge/License-MIT-yellow.svg?style=for-the-badge)
![Testing: Catch2](https://img.shields.io/badge/Testing-Catch2-25c2a0?style=for-the-badge)
![JSON: nlohmann/json](https://img.shields.io/badge/JSON-nlohmann%2Fjson-1f8ceb?style=for-the-badge)

A lightweight C++ console banking application with account creation, update, delete, deposit, withdraw, and robust persistence.

## Features

- Create accounts (Savings and Current)
- View account details
- Modify account name and type
- Deposit and withdraw money
- Delete accounts
- CSV-first persistence on startup
- Automatic JSON synchronization from CSV state
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

- Enter name and account type.
- A date-based account number is generated.
- State is persisted to CSV and synchronized to JSON.

### 2. Display Account

1. Enter account number.
2. View number, name, balance, and account type.

### 3. Modify Account

1. Enter account number.
2. Update name and account type.
3. Changes are persisted immediately.

### 4. Delete Account

1. Enter account number.
2. Account is removed from the in-memory state.
3. CSV and JSON are updated.

### 5. Deposit and Withdraw

1. Enter account number.
2. Enter amount.
3. On valid transactions, changes are persisted to disk.

### 6. Export Accounts

1. Choose export option from the menu.
2. CSV is rewritten from current state and JSON is synchronized.

## Architecture and Persistence Model

- Single source of runtime state:
  - `src/main.cpp` owns one `vector<User>` for consistent state management.
- Startup source of truth:
  - Accounts are loaded from `data/accounts.csv` at program start.
- Disk persistence strategy:
  - Any mutating operation triggers `persist(users)`.
  - `persist(users)` writes `data/accounts.csv` and `data/accounts.json` from the same vector.
- Directory handling:
  - The `data` directory is created automatically if it does not exist.

## Code Explanation

### `User` Class

Core data members:

- account_number
- user_name
- account_balance
- account_type

Main behaviors:

- `createAccount()`: reads name and type input and initializes a new account.
- `displayAccount()`: prints account details.
- `modifyAccount()`: updates name and account type.
- `deposit(double amount)`: credits money to account balance.
- `withdraw(double amount)`: debits money if sufficient balance exists.
- `toJson()`: serializes a `User` object to JSON.

Persistence and synchronization helpers:

- `loadFromCsv()`: startup read path and source of truth.
- `saveToCsv(const std::vector<User>&)`: writes canonical CSV data.
- `loadFromJson()`: JSON read helper used for verification/sync checks.
- `saveToJson(const std::vector<User>&)`: writes synchronized JSON snapshot.
- `persist(const std::vector<User>&)`: writes both CSV and JSON from the same vector.
- `exportToCSV(const std::vector<User>&)`: export entry point that keeps JSON in sync.

### Main Program Flow

- `main()` in `src/main.cpp` loads user state from CSV at startup.
- Menu operations mutate one in-memory `vector<User>` state.
- Any mutation (`create`, `modify`, `delete`, `deposit`, `withdraw`) calls `persist(users)`.
- Export also synchronizes JSON with current CSV-backed state.

## Data Storage Formats

The application stores records in both CSV and JSON. CSV is read at startup, and JSON is maintained as a synchronized mirror.

### CSV Format: data/accounts.csv

```csv
Account Number,Name,Balance,Type
0000202604181,John Doe,1000.50,Savings
0000202604182,Jane Smith,250.00,Current
```

### JSON Format: data/accounts.json

```json
[
  {
    "account_number": "0000202604181",
    "user_name": "John Doe",
    "account_balance": 1000.5,
    "account_type": "Savings"
  },
  {
    "account_number": "0000202604182",
    "user_name": "Jane Smith",
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

### Coverage Highlights

- Unit tests:
  - Deposit behavior
  - Withdraw success and failure behavior
  - Account type serialization
- Integration tests:
  - Storage initialization creates data directory and files
  - CSV startup loading with JSON synchronization
  - CSV parsing with quoted fields
- Regression tests:
  - Persisted updates remain consistent after reload from CSV and JSON

Run the full test suite with:

```sh
make test
```

## Error Handling

- Input validation for menu choices and transaction amounts
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


