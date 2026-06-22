#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ticketbook {

/// Number of seats in every theater (row "a", seats 1–20).
static constexpr int SEATS_PER_THEATER = 20;

/// Represents a movie currently playing.
struct Movie {
    uint32_t    id;     ///< Unique identifier.
    std::string title;  ///< Display title.
};

/// Represents a cinema theater.
struct Theater {
    uint32_t    id;       ///< Unique identifier.
    std::string name;     ///< Display name.
    std::string location; ///< Human-readable location string.
};

/// Outcome codes returned by BookingService::bookSeats().
enum class BookingStatus {
    Success,           ///< All requested seats were successfully booked.
    ShowingNotFound,   ///< No showing exists for the given movie/theater pair.
    InvalidSeat,       ///< One or more seat IDs are invalid or duplicate.
    SeatAlreadyBooked  ///< One or more requested seats are already reserved.
};

/// Result object returned by BookingService::bookSeats().
struct BookingResult {
    BookingStatus            status;       ///< Outcome of the operation.
    std::string              message;      ///< Human-readable description.
    std::vector<std::string> bookedSeats;  ///< Booked seat IDs (populated only on Success).
};

} // namespace ticketbook
