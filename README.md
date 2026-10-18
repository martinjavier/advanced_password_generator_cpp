# Advanced Password Generator (apg) — C++

Command-line password generator written in C++17.

## Usage

```
apg <length> [groups] [-c <n>]
```

| Group | Characters |
|-------|------------|
| `U`   | Uppercase `A-Z` |
| `L`   | Lowercase `a-z` |
| `N`   | Digits `0-9` |
| `S`   | Printable ASCII symbols ``!"#$%&'()*+,-./:;<=>?@[\]^_`{|}~`` |
| `all` | All four groups (default when none is given) |

- Length: 1 to 256.
- Groups are case-insensitive and can be combined or separate (`ULN` = `U L N` = `uln`).
- `-c <n>` generates `n` passwords (1-1000), one per line.
- At least one character from each selected group is guaranteed.
- Uses the operating system's cryptographically secure RNG
  (`getrandom` on Linux, `arc4random_buf` on macOS/BSD, `BCryptGenRandom` on Windows),
  with rejection sampling to avoid modulo bias.

## Examples

```
apg 128 all
apg 16 ULNS
apg 64 NS
apg 32 S
apg 20 -c 5
```

## Build and install

Requirements: a C++17 compiler (GCC 8+, Clang 7+ or MSVC 2019+) and CMake 3.16+.

```
cmake -S . -B build                 # configure (Release by default)
cmake --build build                 # binary at build/apg
ctest --test-dir build              # runs the tests
sudo cmake --install build          # installs 'apg' into /usr/local/bin
```

To install without `sudo`, into `~/.local/bin`:

```
cmake --install build --prefix ~/.local
```

When a password contains symbols, wrap it in single quotes before pasting it into a shell command.
