#include "BookingService.h"

#include <algorithm>
#include <atomic>
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
