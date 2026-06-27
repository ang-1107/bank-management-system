#include "account_behavior.h"
#include "user.h"
#include <algorithm>
#include <cmath>
#include <vector>

// Constants for Savings behavior
namespace {
    constexpr int64_t kRollingWindowSeconds = 24LL * 60LL * 60LL;
    constexpr double kDailyTransactionLimit = 100000.0;
}

// SavingsBehavior implementation

SavingsBehavior::SavingsBehavior(std::vector<TransactionRecord>* txns)
    : recent_transactions(txns) {
}

bool SavingsBehavior::canApplyDeposit(double amount, int64_t nowEpochSeconds) const {
    if (amount <= 0) return false;
    const double currentVolume = rolling24hVolume(nowEpochSeconds);
    return currentVolume + fabs(amount) <= kDailyTransactionLimit;
}

bool SavingsBehavior::canApplyWithdraw(double amount, int64_t nowEpochSeconds) const {
    if (amount <= 0) return false;
    const double currentVolume = rolling24hVolume(nowEpochSeconds);
    return currentVolume + fabs(amount) <= kDailyTransactionLimit;
}

double SavingsBehavior::getCurrentVolume(int64_t nowEpochSeconds) const {
    return rolling24hVolume(nowEpochSeconds);
}

double SavingsBehavior::getRemainingVolume(int64_t nowEpochSeconds) const {
    const double currentVolume = rolling24hVolume(nowEpochSeconds);
    return std::max(0.0, kDailyTransactionLimit - currentVolume);
}

void SavingsBehavior::recordTransaction(double signedAmount, int64_t nowEpochSeconds) {
    recent_transactions->push_back(TransactionRecord{nowEpochSeconds, signedAmount});
}

void SavingsBehavior::onAccountTypeChanged() {
    recent_transactions->clear();
}

AccountBehavior* SavingsBehavior::clone() const {
    return new SavingsBehavior(recent_transactions);
}

void SavingsBehavior::pruneExpiredTransactions(int64_t nowEpochSeconds) {
    const int64_t cutoff = nowEpochSeconds - kRollingWindowSeconds;
    recent_transactions->erase(
        std::remove_if(recent_transactions->begin(), recent_transactions->end(), 
            [&](const TransactionRecord& record) {
                return record.epoch_seconds < cutoff;
            }),
        recent_transactions->end());
}

double SavingsBehavior::rolling24hVolume(int64_t nowEpochSeconds) const {
    const int64_t cutoff = nowEpochSeconds - kRollingWindowSeconds;
    double total = 0.0;
    for (const auto& record : *recent_transactions) {
        if (record.epoch_seconds >= cutoff) {
            total += fabs(record.amount);
        }
    }
    return total;
}

// CurrentBehavior implementation

bool CurrentBehavior::canApplyDeposit(double amount, int64_t /* nowEpochSeconds */) const {
    return amount > 0;
}

bool CurrentBehavior::canApplyWithdraw(double amount, int64_t /* nowEpochSeconds */) const {
    return amount > 0;
}

double CurrentBehavior::getCurrentVolume(int64_t /* nowEpochSeconds */) const {
    return 0.0;
}

double CurrentBehavior::getRemainingVolume(int64_t /* nowEpochSeconds */) const {
    return 0.0;
}

void CurrentBehavior::recordTransaction(double /* signedAmount */, int64_t /* nowEpochSeconds */) {
    // No-op: Current accounts don't track transactions.
}

void CurrentBehavior::onAccountTypeChanged() {
    // No-op: nothing to clean up.
}

AccountBehavior* CurrentBehavior::clone() const {
    return new CurrentBehavior();
}
