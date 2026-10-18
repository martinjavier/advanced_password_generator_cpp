// Unit tests for apg. No external framework needed: run with `ctest`.

#include <algorithm>
#include <cctype>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include "apg.hpp"

using apg::Group;
using Kind = apg::ParseResult::Kind;

namespace {

int failures = 0;
int checks = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        ++checks;                                                                \
        if (!(cond)) {                                                           \
            ++failures;                                                          \
            std::cerr << __FILE__ << ":" << __LINE__ << ": CHECK failed: " #cond \
                      << '\n';                                                   \
        }                                                                        \
    } while (0)

apg::ParseResult parse(std::vector<std::string> args) {
    return apg::parse_args(args);
}

apg::Config cfg(std::vector<std::string> args) {
    auto r = parse(std::move(args));
    if (r.kind != Kind::Generate) {
        std::cerr << "expected Generate, got error: " << r.error << '\n';
        ++failures;
    }
    return r.config;
}

bool is_error(std::vector<std::string> args) {
    return parse(std::move(args)).kind == Kind::Error;
}

const std::vector<Group> ALL(std::begin(apg::ALL_GROUPS), std::end(apg::ALL_GROUPS));

void defaults_to_all_and_one() {
    auto c = cfg({"16"});
    CHECK(c.length == 16);
    CHECK(c.groups == ALL);
    CHECK(c.count == 1);
}

void all_is_case_insensitive() {
    CHECK(cfg({"128", "ALL"}).groups == ALL);
    CHECK(cfg({"128", "all"}).groups == ALL);
    CHECK(cfg({"128", "All"}).groups == ALL);
}

void groups_combined_split_and_lowercase_are_equivalent() {
    auto a = cfg({"16", "ULNS"}).groups;
    auto b = cfg({"16", "U", "L", "N", "S"}).groups;
    auto c = cfg({"16", "snlu"}).groups;
    CHECK(a == b);
    CHECK(a == c);
}

void repeated_groups_are_ignored() {
    CHECK((cfg({"8", "NNS"}).groups == std::vector<Group>{Group::Digits, Group::Symbols}));
}

void count_option_anywhere() {
    CHECK(cfg({"20", "-c", "5"}).count == 5);
    CHECK(cfg({"-c", "3", "20", "NS"}).count == 3);
    CHECK(cfg({"20", "--count", "2"}).count == 2);
}

void length_bounds() {
    CHECK(is_error({"0"}));
    CHECK(is_error({"257"}));
    CHECK(is_error({"abc"}));
    CHECK(is_error({"+5"}));
    CHECK(is_error({"99999999999999999999999"}));
    CHECK(cfg({"1", "S"}).length == 1);
    CHECK(cfg({"256"}).length == 256);
}

void invalid_inputs() {
    CHECK(is_error({}));
    CHECK(is_error({"16", "X"}));
    CHECK(is_error({"16", "-c"}));
    CHECK(is_error({"16", "-c", "0"}));
    CHECK(is_error({"16", "-c", "1001"}));
    CHECK(is_error({"16", "-c", "2", "-c", "3"}));
    CHECK(is_error({"16", "-z"}));
    CHECK(is_error({"2", "all"}));  // 4 groups > length 2
}

void help_and_version() {
    CHECK(parse({"--help"}).kind == Kind::Help);
    CHECK(parse({"16", "-h"}).kind == Kind::Help);
    CHECK(parse({"-V"}).kind == Kind::Version);
}

bool contains(std::string_view set, char c) {
    return set.find(c) != std::string_view::npos;
}

void generated_password_respects_length_and_groups() {
    const std::vector<std::vector<Group>> combos = {
        ALL,
        {Group::Digits, Group::Symbols},
        {Group::Symbols},
        {Group::Upper, Group::Lower},
    };
    for (const auto& groups : combos) {
        for (std::size_t len : {groups.size(), groups.size() + 1, std::size_t{16}, std::size_t{256}}) {
            for (int i = 0; i < 200; ++i) {
                const std::string p = apg::generate(len, groups);
                CHECK(p.size() == len);
                // Every selected group appears at least once.
                for (Group g : groups) {
                    auto set = apg::group_chars(g);
                    CHECK(std::any_of(p.begin(), p.end(), [&](char c) { return contains(set, c); }));
                }
                // No characters outside the selected groups.
                CHECK(std::all_of(p.begin(), p.end(), [&](char c) {
                    return std::any_of(groups.begin(), groups.end(),
                                       [&](Group g) { return contains(apg::group_chars(g), c); });
                }));
            }
        }
    }
}

void random_index_stays_in_range() {
    for (std::size_t bound : {std::size_t{1}, std::size_t{2}, std::size_t{10}, std::size_t{94}}) {
        for (int i = 0; i < 1000; ++i) {
            CHECK(apg::os_random_index(bound) < bound);
        }
    }
}

void symbols_are_all_printable_ascii() {
    // 32 printable ASCII symbols (no space, letters or digits).
    auto s = apg::group_chars(Group::Symbols);
    CHECK(s.size() == 32);
    CHECK(std::all_of(s.begin(), s.end(),
                      [](char c) { return std::ispunct(static_cast<unsigned char>(c)) != 0; }));
}

}  // namespace

int main() {
    const std::vector<std::pair<const char*, std::function<void()>>> tests = {
        {"defaults_to_all_and_one", defaults_to_all_and_one},
        {"all_is_case_insensitive", all_is_case_insensitive},
        {"groups_combined_split_and_lowercase_are_equivalent",
         groups_combined_split_and_lowercase_are_equivalent},
        {"repeated_groups_are_ignored", repeated_groups_are_ignored},
        {"count_option_anywhere", count_option_anywhere},
        {"length_bounds", length_bounds},
        {"invalid_inputs", invalid_inputs},
        {"help_and_version", help_and_version},
        {"generated_password_respects_length_and_groups",
         generated_password_respects_length_and_groups},
        {"random_index_stays_in_range", random_index_stays_in_range},
        {"symbols_are_all_printable_ascii", symbols_are_all_printable_ascii},
    };

    for (const auto& [name, fn] : tests) {
        const int before = failures;
        fn();
        std::cout << (failures == before ? "[ OK ] " : "[FAIL] ") << name << '\n';
    }

    std::cout << "\n" << tests.size() << " tests, " << checks << " checks, " << failures
              << " failures\n";
    return failures == 0 ? 0 : 1;
}
