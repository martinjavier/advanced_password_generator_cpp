// Advanced Password Generator (apg)
//
// Argument parsing and password generation logic.

#include "apg.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <limits>
#include <optional>
#include <stdexcept>

#if defined(_WIN32)
#include <windows.h>
#include <bcrypt.h>
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
#include <stdlib.h>  // arc4random_buf
#else
#include <cerrno>
#include <sys/random.h>  // getrandom
#endif

namespace apg {

namespace {

constexpr std::string_view UPPER = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
constexpr std::string_view LOWER = "abcdefghijklmnopqrstuvwxyz";
constexpr std::string_view DIGITS = "0123456789";
constexpr std::string_view SYMBOLS = "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";

std::optional<Group> group_from_char(char c) {
    switch (std::toupper(static_cast<unsigned char>(c))) {
        case 'U': return Group::Upper;
        case 'L': return Group::Lower;
        case 'N': return Group::Digits;
        case 'S': return Group::Symbols;
        default: return std::nullopt;
    }
}

std::size_t canonical_index(Group g) {
    return static_cast<std::size_t>(g);
}

bool iequals(std::string_view a, std::string_view b) {
    return a.size() == b.size() &&
           std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
               return std::tolower(static_cast<unsigned char>(x)) ==
                      std::tolower(static_cast<unsigned char>(y));
           });
}

/// Parses a non-negative integer, accepting only digits (no sign, no spaces).
std::optional<std::size_t> parse_number(std::string_view s) {
    if (s.empty() || !std::all_of(s.begin(), s.end(),
                                  [](char c) { return c >= '0' && c <= '9'; })) {
        return std::nullopt;
    }
    std::size_t value = 0;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
    if (ec == std::errc::result_out_of_range) {
        return std::numeric_limits<std::size_t>::max();  // will fail the range check
    }
    if (ec != std::errc() || ptr != s.data() + s.size()) {
        return std::nullopt;
    }
    return value;
}

std::string parse_length(std::string_view s, std::size_t& out) {
    auto n = parse_number(s);
    if (!n) {
        return "length must be a number between " + std::to_string(MIN_LEN) + " and " +
               std::to_string(MAX_LEN) + " (got: '" + std::string(s) + "').";
    }
    if (*n < MIN_LEN || *n > MAX_LEN) {
        return "length must be between " + std::to_string(MIN_LEN) + " and " +
               std::to_string(MAX_LEN) + " (got: " + std::string(s) + ").";
    }
    out = *n;
    return {};
}

std::string parse_count(std::string_view s, std::size_t& out) {
    auto n = parse_number(s);
    if (!n) {
        return "-c must be a number between " + std::to_string(MIN_COUNT) + " and " +
               std::to_string(MAX_COUNT) + " (got: '" + std::string(s) + "').";
    }
    if (*n < MIN_COUNT || *n > MAX_COUNT) {
        return "-c must be between " + std::to_string(MIN_COUNT) + " and " +
               std::to_string(MAX_COUNT) + " (got: " + std::string(s) + ").";
    }
    out = *n;
    return {};
}

ParseResult error(std::string msg) {
    ParseResult r;
    r.kind = ParseResult::Kind::Error;
    r.error = std::move(msg);
    return r;
}

/// Fills the buffer with random bytes from the operating system.
void os_random_bytes(void* buf, std::size_t len) {
#if defined(_WIN32)
    if (BCryptGenRandom(nullptr, static_cast<PUCHAR>(buf), static_cast<ULONG>(len),
                        BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) {
        throw std::runtime_error("BCryptGenRandom failed");
    }
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
    arc4random_buf(buf, len);
#else
    auto* p = static_cast<unsigned char*>(buf);
    while (len > 0) {
        ssize_t n = getrandom(p, len, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            throw std::runtime_error("getrandom failed");
        }
        p += n;
        len -= static_cast<std::size_t>(n);
    }
#endif
}

}  // namespace

std::string_view group_chars(Group g) {
    switch (g) {
        case Group::Upper: return UPPER;
        case Group::Lower: return LOWER;
        case Group::Digits: return DIGITS;
        case Group::Symbols: return SYMBOLS;
    }
    return {};
}

ParseResult parse_args(const std::vector<std::string>& args) {
    if (args.empty()) {
        return error("missing password length.");
    }
    for (const auto& a : args) {
        if (a == "-h" || a == "--help") {
            ParseResult r;
            r.kind = ParseResult::Kind::Help;
            return r;
        }
    }
    for (const auto& a : args) {
        if (a == "-V" || a == "--version") {
            ParseResult r;
            r.kind = ParseResult::Kind::Version;
            return r;
        }
    }

    std::optional<std::size_t> length;
    std::optional<std::size_t> count;
    std::vector<Group> groups;
    bool all = false;

    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& arg = args[i];

        if (arg == "-c" || arg == "--count") {
            if (count) {
                return error("option -c can only be given once.");
            }
            if (i + 1 >= args.size()) {
                return error("option -c requires a number (e.g. -c 5).");
            }
            std::size_t n = 0;
            if (auto err = parse_count(args[i + 1], n); !err.empty()) {
                return error(err);
            }
            count = n;
            ++i;
            continue;
        }

        if (arg.size() > 1 && arg[0] == '-') {
            return error("unknown option: '" + arg + "'.");
        }

        if (!length) {
            std::size_t n = 0;
            if (auto err = parse_length(arg, n); !err.empty()) {
                return error(err);
            }
            length = n;
        } else if (iequals(arg, "all")) {
            all = true;
        } else {
            for (char c : arg) {
                auto g = group_from_char(c);
                if (!g) {
                    return error(std::string("unknown group '") + c + "' in '" + arg +
                                 "'. Use U, L, N, S or all.");
                }
                if (std::find(groups.begin(), groups.end(), *g) == groups.end()) {
                    groups.push_back(*g);
                }
            }
        }
    }

    if (!length) {
        return error("missing password length.");
    }

    // No groups (or 'all') means all four.
    if (all || groups.empty()) {
        groups.assign(std::begin(ALL_GROUPS), std::end(ALL_GROUPS));
    } else {
        // Canonical order, so "SU" and "US" produce the same configuration.
        std::sort(groups.begin(), groups.end(), [](Group a, Group b) {
            return canonical_index(a) < canonical_index(b);
        });
    }

    if (*length < groups.size()) {
        return error("length (" + std::to_string(*length) +
                     ") is less than the number of selected groups (" +
                     std::to_string(groups.size()) +
                     "); cannot guarantee one character from each group.");
    }

    ParseResult r;
    r.kind = ParseResult::Kind::Generate;
    r.config.length = *length;
    r.config.groups = std::move(groups);
    r.config.count = count.value_or(1);
    return r;
}

std::size_t os_random_index(std::size_t bound) {
    if (bound == 0) {
        throw std::invalid_argument("bound must be greater than zero");
    }
    // Rejection sampling avoids modulo bias.
    const std::uint64_t b = bound;
    const std::uint64_t limit = std::numeric_limits<std::uint64_t>::max() -
                                (std::numeric_limits<std::uint64_t>::max() % b);
    std::uint64_t x = 0;
    do {
        os_random_bytes(&x, sizeof(x));
    } while (x >= limit);
    return static_cast<std::size_t>(x % b);
}

std::string generate(std::size_t length, const std::vector<Group>& groups) {
    return generate_with(os_random_index, length, groups);
}

std::string generate_with(const RandomIndex& rng, std::size_t length,
                          const std::vector<Group>& groups) {
    if (groups.empty() || length < groups.size()) {
        throw std::invalid_argument("invalid length or groups");
    }

    std::string pool;
    for (Group g : groups) {
        pool += group_chars(g);
    }

    std::string out;
    out.reserve(length);
    for (Group g : groups) {
        auto set = group_chars(g);
        out.push_back(set[rng(set.size())]);
    }
    while (out.size() < length) {
        out.push_back(pool[rng(pool.size())]);
    }

    // Fisher-Yates shuffle with the same secure source.
    for (std::size_t i = out.size() - 1; i > 0; --i) {
        std::swap(out[i], out[rng(i + 1)]);
    }
    return out;
}

std::string help_text() {
    const std::string min_len = std::to_string(MIN_LEN);
    const std::string max_len = std::to_string(MAX_LEN);
    const std::string min_count = std::to_string(MIN_COUNT);
    const std::string max_count = std::to_string(MAX_COUNT);

    return std::string("Advanced Password Generator (apg) v") + VERSION + "\n"
        "\n"
        "USAGE:\n"
        "    apg <length> [groups] [-c <n>]\n"
        "\n"
        "ARGUMENTS:\n"
        "    <length>      Password length (" + min_len + "-" + max_len + ")\n"
        "    [groups]      Group combination (default: all)\n"
        "                    U    Uppercase   (A-Z)\n"
        "                    L    Lowercase   (a-z)\n"
        "                    N    Digits      (0-9)\n"
        "                    S    Symbols     (" + std::string(SYMBOLS) + ")\n"
        "                    all  All four groups (ULNS)\n"
        "                  Case-insensitive, and may be combined or separate:\n"
        "                  'ULN' is the same as 'U L N' or 'uln'.\n"
        "\n"
        "OPTIONS:\n"
        "    -c, --count <n>   Generate n passwords (" + min_count + "-" + max_count + "), one per line\n"
        "    -h, --help        Show this help\n"
        "    -V, --version     Show version\n"
        "\n"
        "At least one character from each selected group is guaranteed.\n"
        "\n"
        "EXAMPLES:\n"
        "    apg 128 all\n"
        "    apg 16 ULNS\n"
        "    apg 64 NS\n"
        "    apg 32 S\n"
        "    apg 20 -c 5\n"
        "    apg 12 U L N -c 3";
}

}  // namespace apg
