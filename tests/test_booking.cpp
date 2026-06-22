#include "BookingService.h"

#include <algorithm>
#include <atomic>
#include <random>   // note: std::mt19937 seeded with a fixed value is used below for deterministic, reproducible sequence, no flaky tests
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

using namespace ticketbook;

// =============================================================================
// Test fixture — a fresh service instance per test case
// =============================================================================

class BookingServiceTest : public ::testing::Test {
protected:
    BookingService service;
};

// =============================================================================
// getMovies
// =============================================================================

TEST_F(BookingServiceTest, GetMoviesReturnsDemoData) {
    EXPECT_EQ(service.getMovies().size(), 3u);
}

TEST_F(BookingServiceTest, GetMoviesContainsExpectedTitles) {
    const auto movies = service.getMovies();
    std::vector<std::string> titles;
    titles.reserve(movies.size());
    for (const auto& m : movies) titles.push_back(m.title);

    EXPECT_NE(std::find(titles.begin(), titles.end(), "Inception"),       titles.end());
    EXPECT_NE(std::find(titles.begin(), titles.end(), "The Dark Knight"), titles.end());
    EXPECT_NE(std::find(titles.begin(), titles.end(), "Interstellar"),    titles.end());
}

// =============================================================================
// getTheatersForMovie
// =============================================================================

TEST_F(BookingServiceTest, InceptionShowsAtTwoTheaters) {
    EXPECT_EQ(service.getTheatersForMovie(1).size(), 2u);
}

TEST_F(BookingServiceTest, InterstellarShowsAtAllThreeTheaters) {
    EXPECT_EQ(service.getTheatersForMovie(3).size(), 3u);
}

TEST_F(BookingServiceTest, UnknownMovieReturnsEmptyTheaterList) {
    EXPECT_TRUE(service.getTheatersForMovie(999).empty());
}

// =============================================================================
// getAvailableSeats
// =============================================================================

TEST_F(BookingServiceTest, AllSeatsAvailableInitially) {
    EXPECT_EQ(service.getAvailableSeats(1, 1).size(),
              static_cast<size_t>(SEATS_PER_THEATER));
}

TEST_F(BookingServiceTest, SeatLabelsAreCorrect) {
    const auto seats = service.getAvailableSeats(1, 1);
    ASSERT_GE(seats.size(), 20u);
    EXPECT_EQ(seats.front(), "a1");
    EXPECT_EQ(seats.back(),  "a20");
}

TEST_F(BookingServiceTest, NoShowingReturnsEmptySeatList) {
    // Inception (id=1) does NOT play at Grand Cinema (id=3).
    EXPECT_TRUE(service.getAvailableSeats(1, 3).empty());
}

// =============================================================================
// bookSeats — success paths
// =============================================================================

TEST_F(BookingServiceTest, BookSingleSeatSucceeds) {
    const auto result = service.bookSeats(1, 1, {"a5"});
    EXPECT_EQ(result.status, BookingStatus::Success);
    ASSERT_EQ(result.bookedSeats.size(), 1u);
    EXPECT_EQ(result.bookedSeats[0], "a5");
}

TEST_F(BookingServiceTest, BookMultipleSeatsSucceeds) {
    const auto result = service.bookSeats(1, 1, {"a1", "a2", "a3"});
    EXPECT_EQ(result.status, BookingStatus::Success);
    EXPECT_EQ(result.bookedSeats.size(), 3u);
}

TEST_F(BookingServiceTest, BookedSeatsDisappearFromAvailableList) {
    service.bookSeats(1, 1, {"a1", "a10"});
    const auto seats = service.getAvailableSeats(1, 1);

    EXPECT_EQ(seats.size(), static_cast<size_t>(SEATS_PER_THEATER - 2));
    EXPECT_EQ(std::find(seats.begin(), seats.end(), "a1"),  seats.end());
    EXPECT_EQ(std::find(seats.begin(), seats.end(), "a10"), seats.end());
}

// =============================================================================
// bookSeats — failure paths
// =============================================================================

TEST_F(BookingServiceTest, BookingAlreadyBookedSeatFails) {
    service.bookSeats(1, 1, {"a3"});
    const auto result = service.bookSeats(1, 1, {"a3"});
    EXPECT_EQ(result.status, BookingStatus::SeatAlreadyBooked);
}

TEST_F(BookingServiceTest, AtomicityNoneBookedWhenOneFails) {
    service.bookSeats(1, 1, {"a1"});   // pre-book a1

    // Request a2 + a1(taken) + a3  →  should fail atomically.
    const auto result = service.bookSeats(1, 1, {"a2", "a1", "a3"});
    EXPECT_EQ(result.status, BookingStatus::SeatAlreadyBooked);

    // a2 and a3 must still be available.
    const auto seats = service.getAvailableSeats(1, 1);
    EXPECT_NE(std::find(seats.begin(), seats.end(), "a2"), seats.end());
    EXPECT_NE(std::find(seats.begin(), seats.end(), "a3"), seats.end());
}

TEST_F(BookingServiceTest, InvalidSeatIdFails) {
    EXPECT_EQ(service.bookSeats(1, 1, {"z99"}).status, BookingStatus::InvalidSeat);
    EXPECT_EQ(service.bookSeats(1, 1, {"a0"}).status,  BookingStatus::InvalidSeat);
    EXPECT_EQ(service.bookSeats(1, 1, {"a21"}).status, BookingStatus::InvalidSeat);
    EXPECT_EQ(service.bookSeats(1, 1, {"1a"}).status,  BookingStatus::InvalidSeat);
}

TEST_F(BookingServiceTest, EmptySeatListFails) {
    EXPECT_EQ(service.bookSeats(1, 1, {}).status, BookingStatus::InvalidSeat);
}

TEST_F(BookingServiceTest, DuplicateSeatInRequestFails) {
    EXPECT_EQ(service.bookSeats(1, 1, {"a1", "a1"}).status,
              BookingStatus::InvalidSeat);
}

TEST_F(BookingServiceTest, BookingForNonExistentShowingFails) {
    // Inception (1) does not play at Grand Cinema (3).
    EXPECT_EQ(service.bookSeats(1, 3, {"a1"}).status,
              BookingStatus::ShowingNotFound);
}

// =============================================================================
// Thread safety
// =============================================================================

TEST_F(BookingServiceTest, ConcurrentBookingSameSeatOnlyOneSucceeds) {
    constexpr int NUM_THREADS = 20;
    std::atomic<int> successCount{0};

    std::vector<std::thread> threads;
    threads.reserve(NUM_THREADS);
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back([&]() {
            if (service.bookSeats(1, 1, {"a1"}).status == BookingStatus::Success) {
                ++successCount;
            }
        });
    }
    for (auto& t : threads) t.join();

    EXPECT_EQ(successCount.load(), 1);
}

TEST_F(BookingServiceTest, ConcurrentBookingDistinctSeatsAllSucceed) {
    std::atomic<int> successCount{0};

    std::vector<std::thread> threads;
    threads.reserve(SEATS_PER_THEATER);
    for (int i = 1; i <= SEATS_PER_THEATER; ++i) {
        threads.emplace_back([&, i]() {
            const std::string seat = "a" + std::to_string(i);
            if (service.bookSeats(1, 1, {seat}).status == BookingStatus::Success) {
                ++successCount;
            }
        });
    }
    for (auto& t : threads) t.join();

    EXPECT_EQ(successCount.load(), SEATS_PER_THEATER);
    EXPECT_TRUE(service.getAvailableSeats(1, 1).empty());
}

TEST_F(BookingServiceTest, ConcurrentReadsDoNotBlockEachOther) {
    // Pre-book a few seats so getAvailableSeats has something to iterate.
    service.bookSeats(1, 1, {"a1", "a2", "a3"});

    constexpr int NUM_READERS = 16;
    std::atomic<int> completedReaders{0};

    std::vector<std::thread> threads;
    threads.reserve(NUM_READERS);
    for (int i = 0; i < NUM_READERS; ++i) {
        threads.emplace_back([&]() {
            const auto seats = service.getAvailableSeats(1, 1);
            EXPECT_EQ(seats.size(), static_cast<size_t>(SEATS_PER_THEATER - 3));
            ++completedReaders;
        });
    }
    for (auto& t : threads) t.join();

    // All readers must have completed — none was starved or deadlocked.
    EXPECT_EQ(completedReaders.load(), NUM_READERS);
}

TEST_F(BookingServiceTest, ConcurrentReadAndWriteDifferentShowings) {
    // Reader on showing (1,1) and writer on showing (3,3) must not block
    // each other — they hold locks on different ShowingData objects.
    std::atomic<bool> readerDone{false};
    std::atomic<bool> writerDone{false};

    std::thread reader([&]() {
        for (int i = 0; i < 100; ++i)
            service.getAvailableSeats(1, 1);
        readerDone = true;
    });

    std::thread writer([&]() {
        for (int i = 1; i <= SEATS_PER_THEATER; ++i)
            service.bookSeats(3, 3, {"a" + std::to_string(i)});
        writerDone = true;
    });

    reader.join();
    writer.join();

    EXPECT_TRUE(readerDone.load());
    EXPECT_TRUE(writerDone.load());
    // Showing (1,1) untouched — all seats still available.
    EXPECT_EQ(service.getAvailableSeats(1, 1).size(),
              static_cast<size_t>(SEATS_PER_THEATER));
    // Showing (3,3) fully booked.
    EXPECT_TRUE(service.getAvailableSeats(3, 3).empty());
}

TEST_F(BookingServiceTest, StressHighContentionNoOverbooking) {
    // All threads target the same narrow set of seats (a1..a4) to maximise
    // mutex contention.  Each thread also interleaves reads (getAvailableSeats)
    // with booking attempts, mirroring real-world mixed workloads.
    //
    // Fixed seed makes the test deterministic and reproducible.
    //
    // Invariant: total successful bookings == number of seats marked booked.

    constexpr int    NUM_THREADS    = 12;
    constexpr int    ITERATIONS     = 50;
    constexpr uint32_t MOVIE_ID     = 1;
    constexpr uint32_t THEATER_ID   = 1;
    // Contested seats — narrow range to guarantee frequent lock collisions.
    const std::vector<std::string> hotSeats = {"a1", "a2", "a3", "a4"};

    std::atomic<int> totalSuccesses{0};

    std::vector<std::thread> threads;
    threads.reserve(NUM_THREADS);

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&, t]() {
            // Each thread gets its own PRNG seeded deterministically.
            std::mt19937 rng(42 + t);
            std::uniform_int_distribution<int> seatDist(
                0, static_cast<int>(hotSeats.size()) - 1);
            std::bernoulli_distribution doRead(0.3); // 30% reads, 70% writes

            for (int i = 0; i < ITERATIONS; ++i) {
                if (doRead(rng)) {
                    // Read path — exercises shared_lock under contention.
                    service.getAvailableSeats(MOVIE_ID, THEATER_ID);
                } else {
                    // Write path — all threads compete for the same 4 seats.
                    const auto& seat = hotSeats[seatDist(rng)];
                    if (service.bookSeats(MOVIE_ID, THEATER_ID, {seat}).status
                            == BookingStatus::Success) {
                        ++totalSuccesses;
                    }
                }
            }
        });
    }

    for (auto& th : threads) th.join();

    // Count how many of the hot seats are actually marked booked.
    const auto available = service.getAvailableSeats(MOVIE_ID, THEATER_ID);
    const int bookedCount = SEATS_PER_THEATER - static_cast<int>(available.size());

    // Core invariant: every reported success corresponds to a real booking.
    EXPECT_EQ(totalSuccesses.load(), bookedCount);
    // At most 4 seats could have been booked (only hotSeats were targeted).
    EXPECT_LE(bookedCount, static_cast<int>(hotSeats.size()));
}
