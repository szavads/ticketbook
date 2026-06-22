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
};

struct BookingService::Impl {
    mutable std::shared_mutex mutex;

    std::vector<Movie>   movies;
    std::vector<Theater> theaters;

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

    // Inception plays at CineMax and Movieplex
    showings.emplace(makeKey(1, 1), ShowingData{});
    showings.emplace(makeKey(1, 2), ShowingData{});

    // The Dark Knight plays at CineMax and Grand
    showings.emplace(makeKey(2, 1), ShowingData{});
    showings.emplace(makeKey(2, 3), ShowingData{});

    // Interstellar plays at all three theaters
    showings.emplace(makeKey(3, 1), ShowingData{});
    showings.emplace(makeKey(3, 2), ShowingData{});
    showings.emplace(makeKey(3, 3), ShowingData{});
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
    std::shared_lock lock(m_impl->mutex);
    return m_impl->movies;
}

std::vector<Theater> BookingService::getTheatersForMovie(uint32_t movieId) const {
    std::shared_lock lock(m_impl->mutex);
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
    std::shared_lock lock(m_impl->mutex);
    const auto it = m_impl->showings.find(makeKey(movieId, theaterId));
    if (it == m_impl->showings.end()) return {};

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

    std::unique_lock lock(m_impl->mutex);

    auto it = m_impl->showings.find(makeKey(movieId, theaterId));
    if (it == m_impl->showings.end()) {
        return {BookingStatus::ShowingNotFound,
                "No showing found for the given movie and theater.", {}};
    }

    ShowingData& showing = it->second;

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
