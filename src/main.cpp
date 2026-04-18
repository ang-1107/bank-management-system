#include "user.h"
#include <algorithm>
#include <csignal>
#include <cstdlib>
#include <limits>

using namespace std;

namespace {
void printExitMessage() {
    cout << "\nExiting... Have a Nice Day!\n";
}

void handleTerminationSignal(int) {
    printExitMessage();
    std::exit(0);
}

bool readMenuChoice(int& choice, int minChoice, int maxChoice) {
    if (!(cin >> choice)) {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "Invalid choice! Please enter a number between " << minChoice << " and " << maxChoice << ".\n";
        return false;
    }
    if (choice < minChoice || choice > maxChoice) {
        cout << "Invalid choice! Please enter a number between " << minChoice << " and " << maxChoice << ".\n";
        return false;
    }
    return true;
}

bool runAuthenticatedSession(vector<User>& users, size_t currentIndex) {
    int choice;
    bool loggedIn = true;

    while (loggedIn) {
        cout << "\n==== ACCOUNT MENU ====\n";
        cout << "1. View Account\n";
        cout << "2. Modify Account\n";
        cout << "3. Deposit Money\n";
        cout << "4. Withdraw Money\n";
        cout << "5. Logout\n";
        cout << "Enter your choice: ";

        if (!readMenuChoice(choice, 1, 5)) {
            continue;
        }

        switch (choice) {
            case 1:
                users[currentIndex].displayAccount();
                if (users[currentIndex].getAccountType() == SAVINGS) {
                    cout << "24h Transaction Volume: $" << fixed << setprecision(2)
                         << users[currentIndex].getCurrent24hVolume() << endl;
                    cout << "Remaining 24h Volume: $" << fixed << setprecision(2)
                         << users[currentIndex].getRemaining24hVolume() << endl;
                }
                break;
            case 2:
                users[currentIndex].modifyAccount();
                if (!User::persist(users)) {
                    cerr << "Error: Failed to persist modified account to disk." << endl;
                }
                break;
            case 3: {
                double amount;
                cout << "Enter Amount to Deposit: ";
                if (!(cin >> amount)) {
                    cin.clear();
                    cin.ignore(numeric_limits<streamsize>::max(), '\n');
                    cout << "Invalid amount!" << endl;
                    break;
                }

                users[currentIndex].deposit(amount);
                if (!User::persist(users)) {
                    cerr << "Error: Failed to persist deposit transaction to disk." << endl;
                }
                break;
            }
            case 4: {
                double amount;
                cout << "Enter Amount to Withdraw: ";
                if (!(cin >> amount)) {
                    cin.clear();
                    cin.ignore(numeric_limits<streamsize>::max(), '\n');
                    cout << "Invalid amount!" << endl;
                    break;
                }

                if (users[currentIndex].withdraw(amount)) {
                    if (!User::persist(users)) {
                        cerr << "Error: Failed to persist withdrawal transaction to disk." << endl;
                    }
                }
                break;
            }
            case 5:
                cout << "Logged out successfully." << endl;
                loggedIn = false;
                break;
            default:
                break;
        }
    }

    return true;
}
}  // namespace

int main() {
    signal(SIGINT, handleTerminationSignal);
#ifdef SIGTERM
    signal(SIGTERM, handleTerminationSignal);
#endif

    vector<User> users = User::loadFromCsv();  // CSV is the source of truth at startup

    int choice;
    bool isRunning = true;

    while (isRunning) {
        cout << "\n==== WELCOME TO BANK MANAGEMENT SYSTEM ====\n";
        cout << "1. Create Account\n";
        cout << "2. Login\n";
        cout << "3. Exit\n";
        cout << "Enter your choice: ";

        if (!readMenuChoice(choice, 1, 3)) {
            continue;
        }

        switch (choice) {
            case 1: {
                User newUser;
                newUser.createAccount();
                users.push_back(newUser);
                if (!User::persist(users)) {
                    cerr << "Error: Failed to persist account creation to disk." << endl;
                }

                runAuthenticatedSession(users, users.size() - 1);
                break;
            }
            case 2: {
                string accNumber;
                string password;

                cout << "Enter Account Number: ";
                cin >> accNumber;
                cout << "Enter Password: ";
                getline(cin >> ws, password);

                auto it = find_if(users.begin(), users.end(), [&](const User& u) {
                    return u.getAccountNumber() == accNumber;
                });

                if (it == users.end()) {
                    cout << "Account Not Found!" << endl;
                    break;
                }

                if (!it->verifyPassword(password)) {
                    cout << "Invalid credentials!" << endl;
                    break;
                }

                cout << "Login successful." << endl;
                size_t currentIndex = static_cast<size_t>(distance(users.begin(), it));
                runAuthenticatedSession(users, currentIndex);
                break;
            }
            case 3:
                printExitMessage();
                isRunning = false;
                break;
            default:
                break;
        }
    }

    return 0;
}
