#pragma once

#include <string_view>

namespace we::string {

// These functions ignore (A-Z and a-z but nothing else, this is good enough for our use case).

/// @brief Compare two strings for equality, ignoring simple casing.
/// @param left The left string to compare.
/// @param right The right string to comapre.
/// @return If the strings are equal.
bool iequals(const std::string_view left, const std::string_view right) noexcept;

/// @brief Check if left starts with right, ignoring simple casing.
/// @param left The left string to check.
/// @param right The right string to check.
/// @return If left starts with right.
bool istarts_with(const std::string_view left, const std::string_view right) noexcept;

}