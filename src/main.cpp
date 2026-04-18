#include "user.h"
#include <algorithm>
#include <limits>

using namespace std;

int main() {
    vector<User> users = User::loadFromCsv();  // CSV is the source of truth at startup

    int choice;
    do {
        cout << "\n==== WELCOME TO BANK MANAGEMENT SYSTEM ====\n";
        cout << "1. Create Account\n";
        cout << "2. Display Account\n";
        cout << "3. Modify Account\n";
        cout << "4. Delete Account\n";
        cout << "5. Deposit Money\n";
        cout << "6. Withdraw Money\n";
        cout << "7. Export Accounts to CSV\n";
        cout << "8. Exit\n";
        cout << "Enter your choice: ";
        if (!(cin >> choice)) {
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            cout << "Invalid choice! Please enter a number between 1 and 8.\n";
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
                break;
            }
            case 2: {
                string accNumber;
                cout << "Enter Account Number: ";
                cin >> accNumber;

                auto it = find_if(users.begin(), users.end(), [&](const User& u) {
                    return u.getAccountNumber() == accNumber;
                });

                if (it != users.end()) {
                    it->displayAccount();
                } else {
                    cout << "Account Not Found!" << endl;
                }
                break;
            }
            case 3: {
                string accNumber;
                cout << "Enter Account Number: ";
                cin >> accNumber;

                auto it = find_if(users.begin(), users.end(), [&](const User& u) {
                    return u.getAccountNumber() == accNumber;
                });

                if (it != users.end()) {
                    it->modifyAccount();
                    if (!User::persist(users)) {
                        cerr << "Error: Failed to persist modified account to disk." << endl;
                    }
                } else {
                    cout << "Account Not Found!" << endl;
                }
                break;
            }
            case 4: {
                string accNumber;
                cout << "Enter Account Number: ";
                cin >> accNumber;

                auto it = find_if(users.begin(), users.end(), [&](const User& u) {
                    return u.getAccountNumber() == accNumber;
                });

                if (it != users.end()) {
                    users.erase(it);  // Remove from vector
                    if (!User::persist(users)) {
                        cerr << "Error: Failed to persist account deletion to disk." << endl;
                    }
                    cout << "Account deleted successfully." << endl;
                } else {
                    cout << "Account Not Found!" << endl;
                }
                break;
            }
            case 5: {
                string accNumber;
                double amount;
                cout << "Enter Account Number: ";
                cin >> accNumber;
                cout << "Enter Amount to Deposit: ";
                if (!(cin >> amount)) {
                    cin.clear();
                    cin.ignore(numeric_limits<streamsize>::max(), '\n');
                    cout << "Invalid amount!" << endl;
                    break;
                }

                auto it = find_if(users.begin(), users.end(), [&](const User& u) {
                    return u.getAccountNumber() == accNumber;
                });

                if (it != users.end()) {
                    it->deposit(amount);
                    if (!User::persist(users)) {
                        cerr << "Error: Failed to persist deposit transaction to disk." << endl;
                    }
                } else {
                    cout << "Account Not Found!" << endl;
                }
                break;
            }
            case 6: {
                string accNumber;
                double amount;
                cout << "Enter Account Number: ";
                cin >> accNumber;
                cout << "Enter Amount to Withdraw: ";
                if (!(cin >> amount)) {
                    cin.clear();
                    cin.ignore(numeric_limits<streamsize>::max(), '\n');
                    cout << "Invalid amount!" << endl;
                    break;
                }

                auto it = find_if(users.begin(), users.end(), [&](const User& u) {
                    return u.getAccountNumber() == accNumber;
                });

                if (it != users.end()) {
                    if (it->withdraw(amount)) {
                        if (!User::persist(users)) {
                            cerr << "Error: Failed to persist withdrawal transaction to disk." << endl;
                        }
                    }
                } else {
                    cout << "Account Not Found!" << endl;
                }
                break;
            }
            case 7: {
                if (!User::exportToCSV(users)) {
                    cerr << "Error: CSV export failed." << endl;
                }
                break;
            }
            case 8:
                cout << "Exiting... Have a Nice Day!\n";
                break;
            default:
                cout << "Invalid choice! Please try again.\n";
        }
    } while (choice != 8);

    return 0;
}
