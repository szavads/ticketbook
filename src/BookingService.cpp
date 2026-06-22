#include "BookingService.h"

#include <algorithm>
#include <array>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace ticketbook {

// =============================================================================
// Internal helpers
// =============================================================================

namespace {

/// Encode a (movieId, theaterId) pair as a single 64-bit map key.
constexpr uint64_t makeKey(uint32_t movieId, uint32_t theaterId) noexcept {
    return (static_cast<uint64_t>(movieId) << 32) | theaterId;
}

/// Convert a seat ID such as "a3" to a 0-based array index (0..SEATS_PER_THEATER-1).
/// Returns -1 for malformed or out-of-range IDs.
int seatIdToIndex(const std::string& seatId) noexcept {
    if (seatId.size() < 2 || seatId.front() != 'a') return -1;
    try {
        const int n = std::stoi(seatId.substr(1));
        return (n >= 1 && n <= SEATS_PER_THEATER) ? (n - 1) : -1;
    } catch (...) {
        return -1;
    }
}

/// Convert a 0-based index back to a seat ID string.
inline std::string indexToSeatId(int idx) {
    return "a" + std::to_string(idx + 1);
}

} // anonymous namespace

// =============================================================================
// Internal data structures (pImpl)
// =============================================================================

/// Per-showing booking state.
struct ShowingData {
    /// Booked flags: index 0 → "a1", index 19 → "a20".
    std::array<bool, SEATS_PER_THEATER> booked{};

    /// Guards booked[] only. The ShowingData object itself is never
    /// moved or erased after construction, so taking a pointer/reference
    /// to it and then locking is safe without a global map lock.
    mutable std::shared_mutex mutex;
};

struct BookingService::Impl {
    // Immutable after construction — populated once in seedData(), never modified.
    // Safe to read from any thread without a lock.
    std::vector<Movie>   movies;
    std::vector<Theater> theaters;

    // Keys are immutable after construction (same guarantee as movies/theaters).
    // Only ShowingData::booked[] is mutable and requires synchronisation.
    /// Key: makeKey(movieId, theaterId)  →  ShowingData
    std::unordered_map<uint64_t, ShowingData> showings;

    void seedData();
};

void BookingService::Impl::seedData() {
    movies = {
        {1, "Inception"},
        {2, "The Dark Knight"},
        {3, "Interstellar"}
    };

    theaters = {
        {1, "CineMax Downtown",  "123 Main St"},
        {2, "Movieplex North",   "456 North Ave"},
        {3, "Grand Cinema",      "789 Grand Blvd"}
    };

    // try_emplace constructs ShowingData in-place — required because
    // ShowingData contains a std::shared_mutex which is not movable.
    showings.try_emplace(makeKey(1, 1));
    showings.try_emplace(makeKey(1, 2));

    // The Dark Knight plays at CineMax and Grand
    showings.try_emplace(makeKey(2, 1));
    showings.try_emplace(makeKey(2, 3));

    // Interstellar plays at all three theaters
    showings.try_emplace(makeKey(3, 1));
    showings.try_emplace(makeKey(3, 2));
    showings.try_emplace(makeKey(3, 3));
}

// =============================================================================
// BookingService
// =============================================================================

BookingService::BookingService()
    : m_impl(std::make_unique<Impl>())
{
    m_impl->seedData();
}

BookingService::~BookingService() = default;

std::vector<Movie> BookingService::getMovies() const {
    // movies is immutable after construction — no lock needed.
    return m_impl->movies;
}

std::vector<Theater> BookingService::getTheatersForMovie(uint32_t movieId) const {
    // theaters and showings keys are immutable after construction — no lock needed.
    std::vector<Theater> result;
    for (const auto& theater : m_impl->theaters) {
        if (m_impl->showings.count(makeKey(movieId, theater.id))) {
            result.push_back(theater);
        }
    }
    return result;
}

std::vector<std::string> BookingService::getAvailableSeats(
    uint32_t movieId, uint32_t theaterId) const
{
    // showings keys are immutable — find() needs no global lock.
    const auto it = m_impl->showings.find(makeKey(movieId, theaterId));
    if (it == m_impl->showings.end()) return {};

    // booked[] is mutable — use a shared (read) lock on this showing only.
    std::shared_lock lock(it->second.mutex);
    std::vector<std::string> seats;
    for (int i = 0; i < SEATS_PER_THEATER; ++i) {
        if (!it->second.booked[i]) {
            seats.push_back(indexToSeatId(i));
        }
    }
    return seats;
}

BookingResult BookingService::bookSeats(
    uint32_t movieId,
    uint32_t theaterId,
    const std::vector<std::string>& seatIds)
{
    if (seatIds.empty()) {
        return {BookingStatus::InvalidSeat, "No seats specified.", {}};
    }

    // showings keys are immutable — find() needs no global lock.
    auto it = m_impl->showings.find(makeKey(movieId, theaterId));
    if (it == m_impl->showings.end()) {
        return {BookingStatus::ShowingNotFound,
                "No showing found for the given movie and theater.", {}};
    }

    // Lock only this showing's mutex exclusively (write lock).
    // Concurrent bookings in other showings proceed in parallel.
    ShowingData& showing = it->second;
    std::unique_lock lock(showing.mutex);

    // ── Phase 1: validate every seat before touching any ─────────────────────
    std::vector<int> indices;
    indices.reserve(seatIds.size());

    for (const auto& seatId : seatIds) {
        const int idx = seatIdToIndex(seatId);
        if (idx < 0) {
            return {BookingStatus::InvalidSeat,
                    "Invalid seat ID: \"" + seatId + "\".", {}};
        }
        if (showing.booked[idx]) {
            return {BookingStatus::SeatAlreadyBooked,
                    "Seat is already booked: \"" + seatId + "\".", {}};
        }
        indices.push_back(idx);
    }

    // Detect duplicates within the same request.
    auto sorted = indices;
    std::sort(sorted.begin(), sorted.end());
    if (std::unique(sorted.begin(), sorted.end()) != sorted.end()) {
        return {BookingStatus::InvalidSeat,
                "Duplicate seat IDs in request.", {}};
    }

    // ── Phase 2: commit ───────────────────────────────────────────────────────
    for (const int idx : indices) {
        showing.booked[idx] = true;
    }

    return {BookingStatus::Success, "Seats booked successfully.", seatIds};
}

} // namespace ticketbook
