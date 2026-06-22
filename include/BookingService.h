#pragma once

#include "Models.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ticketbook {

/**
 * @brief Thread-safe backend service for movie ticket booking.
 *
 * Provides all operations needed to browse the current movie catalogue,
 * discover theaters showing a selected film, inspect available seats, and
 * reserve seats.
 *
 * ### Thread safety
 * All public methods are safe to call concurrently from multiple threads.
 *
 * `getMovies` and `getTheatersForMovie` access only immutable data (populated
 * once at construction) and therefore require no synchronisation at all.
 *
 * `getAvailableSeats` acquires a **shared (read) lock** scoped to the requested
 * showing only, so concurrent seat queries for any showings proceed in parallel.
 *
 * `bookSeats` acquires an **exclusive lock** scoped to the requested showing.
 * Bookings in different showings are fully concurrent; only bookings targeting
 * the same showing are serialised.  This single-level locking scheme is
 * deadlock-free by construction.
 *
 * ### Atomicity
 * `bookSeats` is all-or-nothing: every requested seat is validated before any
 * seat is committed.  If a single seat in the list is invalid or already
 * reserved, no seats are changed.
 *
 * ### Storage
 * All data is kept in memory.  The default constructor seeds the service with
 * a built-in demo dataset (3 movies, 3 theaters, 7 showings).
 */
class BookingService {
public:
    /// Construct the service and populate it with built-in demo data.
    BookingService();

    ~BookingService();

    // Non-copyable, non-movable (owns a mutex).
    BookingService(const BookingService&) = delete;
    BookingService& operator=(const BookingService&) = delete;

    /**
     * @brief Return all movies currently registered in the system.
     * @return Snapshot vector of Movie objects.
     */
    std::vector<Movie> getMovies() const;

    /**
     * @brief Return all theaters that have a showing of the given movie.
     *
     * @param movieId  Identifier of the movie to look up.
     * @return Vector of Theater objects; empty if the movie has no showings or
     *         does not exist.
     */
    std::vector<Theater> getTheatersForMovie(uint32_t movieId) const;

    /**
     * @brief Return available (un-booked) seat identifiers for a showing.
     *
     * Seat identifiers follow the pattern `"a1"` … `"a20"`.
     *
     * @param movieId    Identifier of the movie.
     * @param theaterId  Identifier of the theater.
     * @return Vector of available seat ID strings; empty when the showing does
     *         not exist or every seat is already booked.
     */
    std::vector<std::string> getAvailableSeats(uint32_t movieId,
                                               uint32_t theaterId) const;

    /**
     * @brief Attempt to book one or more seats for a showing.
     *
     * The operation is atomic: all requested seats are validated first; if
     * any seat is unknown or already booked the call returns immediately
     * without modifying any state.
     *
     * @param movieId    Identifier of the movie.
     * @param theaterId  Identifier of the theater.
     * @param seatIds    Seat identifiers to reserve (e.g. `{"a1", "a3"}`).
     * @return BookingResult describing the outcome.
     */
    BookingResult bookSeats(uint32_t movieId,
                            uint32_t theaterId,
                            const std::vector<std::string>& seatIds);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace ticketbook
