#pragma once
#include <string>
#include <vector>
#include <cstdint>

// Shared utility helpers for hashing, CSV handling, and cooldown formatting.
std::string sha256(const std::string& input);
std::string escapeCsvField(const std::string& field);
std::vector<std::string> splitCsvLine(const std::string& line);
std::string formatCooldownTime(int64_t remainingSeconds);
