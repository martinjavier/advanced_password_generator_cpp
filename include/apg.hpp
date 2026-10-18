// Advanced Password Generator (apg)
//
// Argument parsing and password generation logic.

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace apg {

inline constexpr std::size_t MIN_LEN = 1;
inline constexpr std::size_t MAX_LEN = 256;
inline constexpr std::size_t MIN_COUNT = 1;
inline constexpr std::size_t MAX_COUNT = 1000;

inline constexpr const char* VERSION = "0.1.0";

/// Available character groups.
enum class Group { Upper, Lower, Digits, Symbols };

/// All four groups, in canonical order.
inline constexpr Group ALL_GROUPS[] = {Group::Upper, Group::Lower, Group::Digits, Group::Symbols};

/// Characters belonging to a group.
std::string_view group_chars(Group g);

/// Configuration built from the arguments.
struct Config {
    std::size_t length = 0;
    std::vector<Group> groups;
    std::size_t count = 1;
};

/// Parse result: help, version, a valid configuration, or an error.
struct ParseResult {
    enum class Kind { Help, Version, Generate, Error };
    Kind kind = Kind::Error;
    Config config;
    std::string error;
};

/// Parses the arguments (without the program name).
ParseResult parse_args(const std::vector<std::string>& args);

/// Returns a uniformly distributed random number in [0, bound).
using RandomIndex = std::function<std::size_t(std::size_t bound)>;

/// Cryptographically secure random index using the operating system's RNG.
std::size_t os_random_index(std::size_t bound);

/// Generates a password using a cryptographically secure RNG.
///
/// Guarantees at least one character from each group and shuffles the result
/// so those characters are not always at the start.
std::string generate(std::size_t length, const std::vector<Group>& groups);

/// Same as generate(), with an injectable random source (used by tests).
std::string generate_with(const RandomIndex& rng, std::size_t length,
                          const std::vector<Group>& groups);

/// Text shown by --help.
std::string help_text();

}  // namespace apg
