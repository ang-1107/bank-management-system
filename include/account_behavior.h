#pragma once
#include <string>
#include <vector>
#include <cstdint>

// Forward declarations
struct TransactionRecord;

// Abstract base class for account type-specific behavior.
// Encapsulates rules that differ between Savings and Current accounts.
class AccountBehavior {
public:
    virtual ~AccountBehavior() = default;

    // Check if a deposit of the given amount can proceed.
    virtual bool canApplyDeposit(double amount, int64_t nowEpochSeconds) const = 0;

    // Check if a withdrawal of the given amount can proceed.
    virtual bool canApplyWithdraw(double amount, int64_t nowEpochSeconds) const = 0;

    // Get the current 24-hour rolling transaction volume (Savings only; Current returns 0).
    virtual double getCurrentVolume(int64_t nowEpochSeconds) const = 0;

    // Get the remaining allowable volume in the 24-hour window (Savings only; Current returns 0).
    virtual double getRemainingVolume(int64_t nowEpochSeconds) const = 0;

    // Record a transaction (e.g., when deposit/withdraw succeeds).
    // For Current accounts, this is a no-op.
    virtual void recordTransaction(double signedAmount, int64_t nowEpochSeconds) = 0;

    // Called when the account type switches. Allows cleanup (e.g., clear transaction history).
    virtual void onAccountTypeChanged() = 0;

    // Clone this behavior to a new instance (used when swapping behaviors on type change).
    virtual AccountBehavior* clone() const = 0;
};

// Savings account behavior: enforces rolling 24-hour volume limit.
class SavingsBehavior : public AccountBehavior {
private:
    std::vector<TransactionRecord>* recent_transactions;

public:
    explicit SavingsBehavior(std::vector<TransactionRecord>* txns);
    ~SavingsBehavior() override = default;

    bool canApplyDeposit(double amount, int64_t nowEpochSeconds) const override;
    bool canApplyWithdraw(double amount, int64_t nowEpochSeconds) const override;
    double getCurrentVolume(int64_t nowEpochSeconds) const override;
    double getRemainingVolume(int64_t nowEpochSeconds) const override;
    void recordTransaction(double signedAmount, int64_t nowEpochSeconds) override;
    void onAccountTypeChanged() override;
    AccountBehavior* clone() const override;

private:
    void pruneExpiredTransactions(int64_t nowEpochSeconds);
    double rolling24hVolume(int64_t nowEpochSeconds) const;
};

// Current account behavior: no volume enforcement, no transaction tracking.
class CurrentBehavior : public AccountBehavior {
public:
    CurrentBehavior() = default;
    ~CurrentBehavior() override = default;

    bool canApplyDeposit(double amount, int64_t nowEpochSeconds) const override;
    bool canApplyWithdraw(double amount, int64_t nowEpochSeconds) const override;
    double getCurrentVolume(int64_t nowEpochSeconds) const override;
    double getRemainingVolume(int64_t nowEpochSeconds) const override;
    void recordTransaction(double signedAmount, int64_t nowEpochSeconds) override;
    void onAccountTypeChanged() override;
    AccountBehavior* clone() const override;
};
