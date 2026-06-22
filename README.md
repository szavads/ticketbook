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

### Example session

```
========================================
     Movie Ticket Booking System
========================================
----------------------------------------
 Movies Now Playing
----------------------------------------
  [1] Inception
  [2] The Dark Knight
  [3] Interstellar
  [0] Quit
Select movie: 1
----------------------------------------
 Theaters Showing This Movie
----------------------------------------
  [1] CineMax Downtown  —  123 Main St
  [2] Movieplex North   —  456 North Ave
  [0] Back
Select theater: 1
----------------------------------------
 Available Seats
----------------------------------------
  a1  a2  a3  a4  a5  a6  a7  a8  a9  a10
  a11  a12  a13  a14  a15  a16  a17  a18  a19  a20
  Book seats? [y/n]: y
  Seats (space-separated, e.g. a1 a2 a5): a3 a7
----------------------------------------
  Seats booked successfully.
  Booked: a3 a7
```

## Tests

```sh
ctest --test-dir build --build-config Release --output-on-failure
```

Or run the test binary directly:

```sh
# Linux / macOS
./build/tests/ticketbook_tests

# Windows
.\build\tests\Release\ticketbook_tests.exe
```

19 tests across three areas:

| Group | Tests | What is covered |
|---|---|---|
| Data queries | 8 | `getMovies`, `getTheatersForMovie`, `getAvailableSeats` — happy path and not-found cases |
| Booking logic | 8 | Single/multi seat booking, atomicity, already-booked, invalid ID, duplicate in request, no showing |
| Concurrency | 2 | 20 threads racing for the same seat (exactly 1 wins); 20 threads booking distinct seats (all 20 succeed) |

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

### Design decisions

**`std::shared_mutex` over plain `std::mutex`**  
Read operations (`getMovies`, `getTheatersForMovie`, `getAvailableSeats`) hold a shared lock and run in parallel. Only `bookSeats` takes an exclusive lock. This is the right trade-off for a read-heavy workload where multiple UI clients browse movies simultaneously.

**pImpl (Pointer to Implementation)**  
The `BookingService` header exposes zero implementation details — no STL containers, no mutex, no internal types. Consumers depend only on `BookingService.h` and `Models.h`. Changing the internal storage structure requires recompiling only `BookingService.cpp`, not every translation unit that includes the header.

**`makeKey(movieId, theaterId)` — packing two `uint32_t` into one `uint64_t`**  
Using a single integer key in `unordered_map` is faster than `std::pair<uint32_t, uint32_t>` because it avoids a custom hasher and compares in one instruction. The bit shift `(movieId << 32) | theaterId` is lossless since both IDs fit in 32 bits.

**Validate-then-commit booking**  
All seats are checked before any seat is marked. If one seat in a multi-seat request is taken, no seats change — the operation is all-or-nothing. This is equivalent to a serialisable database transaction, achieved here with a single exclusive lock scope.

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
