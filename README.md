# ticketbook

[![CI](https://github.com/szavads/ticketbook/actions/workflows/ci.yml/badge.svg)](https://github.com/szavads/ticketbook/actions/workflows/ci.yml)

A C++17 backend service for online movie ticket booking.
Thread-safe, in-memory, no external database required.

## Features

- View all movies currently playing
- Browse theaters showing a selected film
- Inspect available seats per showing (`a1`–`a20`, 20 seats per theater)
- Book one or more seats atomically — no over-bookings under concurrent load

## Prerequisites

| Tool | Version |
|------|---------|
| CMake | ≥ 3.15 |
| C++ compiler | GCC 9 / Clang 10 / MSVC 2019 or later (C++17) |
| Python | ≥ 3.8 |
| Conan | ≥ 2.0 |

Install Conan and auto-detect your compiler profile:

```sh
pip install "conan>=2.0"
conan profile detect
```

## Build

```sh
# 1. Install dependencies
conan install . --output-folder=build --build=missing -s build_type=Release

# 2. Configure
cmake -B build \
      -DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake \
      -DCMAKE_BUILD_TYPE=Release

# 3. Build
cmake --build build --config Release
```

The CLI binary is placed at:
- `build/ticketbook` — Linux / macOS
- `build\Release\ticketbook.exe` — Windows (MSVC)

## Run

```sh
# Linux / macOS
./build/ticketbook

# Windows
.\build\Release\ticketbook.exe

# Docker
docker build -t ticketbook .
docker run -it ticketbook
```

The interactive menu walks you through selecting a movie, a theater,
viewing available seats, and booking them.

## Tests

```sh
cd build && ctest --output-on-failure
```

Or run the test binary directly:

```sh
# Linux / macOS
./build/tests/ticketbook_tests

# Windows
.\build\tests\Release\ticketbook_tests.exe
```

## API Documentation (Doxygen)

```sh
doxygen Doxyfile
```

HTML output is written to `docs/html/`. Open `docs/html/index.html` in a browser.

## Project structure

```
ticketbook/
├── CMakeLists.txt          # Root CMake configuration
├── conanfile.txt           # Conan 2 dependency manifest (gtest)
├── Dockerfile              # Multi-stage Docker build
├── Doxyfile                # Doxygen configuration
├── include/
│   ├── Models.h            # Data types: Movie, Theater, BookingResult …
│   └── BookingService.h    # Public API — documented header for consumers
├── src/
│   ├── BookingService.cpp  # Thread-safe implementation (pImpl)
│   └── main.cpp            # Interactive CLI frontend
└── tests/
    ├── CMakeLists.txt
    └── test_booking.cpp    # Google Test unit + concurrency tests
```

## Architecture

```
CLI  (src/main.cpp)
       │
       ▼
BookingService          ← public API, pImpl hides implementation
       │
       ▼
In-memory store         ← unordered_map guarded by std::shared_mutex
```

**Reads** (`getMovies`, `getTheatersForMovie`, `getAvailableSeats`) acquire a
**shared lock** and can execute in parallel.

**Writes** (`bookSeats`) acquire an **exclusive lock**.  The booking follows a
two-phase protocol within that lock:
1. **Validate** — check every requested seat before touching any.
2. **Commit** — mark all seats as booked.

This guarantees all-or-nothing semantics without a database transaction.

---

## Reflection

**Most interesting** — Designing correct concurrent booking semantics without a
database.  Using `std::shared_mutex` (shared reads, exclusive writes) combined
with a validate-then-commit pattern delivers no-overbook guarantees with minimal
contention.  Getting the atomicity right — especially handling the "duplicate
seat in one request" edge case — was the most satisfying part of the exercise.

**Most cumbersome** — Cross-platform build chain setup.  Conan 2's CMake
toolchain injection works well once configured, but the difference between
single-config generators (Makefiles / Ninja) and multi-config generators (MSVC)
means binary output paths differ, which requires extra care in documentation and
Docker scripts.
