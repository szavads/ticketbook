#include "BookingService.h"

#include <algorithm>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

using namespace ticketbook;

// =============================================================================
// Display helpers
// =============================================================================

namespace {

void printSeparator() {
    std::cout << "----------------------------------------\n";
}

void printMovies(const std::vector<Movie>& movies) {
    printSeparator();
    std::cout << " Movies Now Playing\n";
    printSeparator();
    for (const auto& m : movies) {
        std::cout << "  [" << m.id << "] " << m.title << "\n";
    }
    std::cout << "  [0] Quit\n";
}

void printTheaters(const std::vector<Theater>& theaters) {
    printSeparator();
    std::cout << " Theaters Showing This Movie\n";
    printSeparator();
    if (theaters.empty()) {
        std::cout << "  (no showings found)\n";
        return;
    }
    for (const auto& t : theaters) {
        std::cout << "  [" << t.id << "] " << t.name
                  << "  —  " << t.location << "\n";
    }
    std::cout << "  [0] Back\n";
}

void printSeats(const std::vector<std::string>& seats) {
    printSeparator();
    std::cout << " Available Seats\n";
    printSeparator();
    if (seats.empty()) {
        std::cout << "  (all seats are booked)\n";
        return;
    }
    std::cout << "  ";
    for (size_t i = 0; i < seats.size(); ++i) {
        std::cout << seats[i];
        if (i + 1 < seats.size()) {
            std::cout << "  ";
            if ((i + 1) % 10 == 0) std::cout << "\n  ";
        }
    }
    std::cout << "\n";
}

// ── Input helpers ─────────────────────────────────────────────────────────────

uint32_t readId(const std::string& prompt) {
    while (true) {
        std::cout << prompt;
        uint32_t id = 0;
        if (std::cin >> id) return id;
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cout << "  Please enter a valid number.\n";
    }
}

std::vector<std::string> readSeatList() {
    // Consume the newline left by the previous std::cin >> operation.
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::cout << "  Seats (space-separated, e.g. a1 a2 a5): ";
    std::string line;
    std::getline(std::cin, line);

    std::vector<std::string> seats;
    std::istringstream iss(line);
    std::string token;
    while (iss >> token) {
        seats.push_back(token);
    }
    return seats;
}

} // anonymous namespace

// =============================================================================
// main
// =============================================================================

int main() {
    BookingService service;

    std::cout << "========================================\n";
    std::cout << "     Movie Ticket Booking System\n";
    std::cout << "========================================\n";

    while (true) {
        // ── Step 1: choose a movie ─────────────────────────────────────────
        const auto movies = service.getMovies();
        printMovies(movies);

        const uint32_t movieId = readId("Select movie: ");
        if (movieId == 0) break;

        const auto movieIt = std::find_if(movies.begin(), movies.end(),
            [movieId](const Movie& m) { return m.id == movieId; });
        if (movieIt == movies.end()) {
            std::cout << "  Unknown movie ID.\n";
            continue;
        }

        // ── Step 2: choose a theater ───────────────────────────────────────
        const auto theaters = service.getTheatersForMovie(movieId);
        printTheaters(theaters);
        if (theaters.empty()) continue;

        const uint32_t theaterId = readId("Select theater: ");
        if (theaterId == 0) continue;

        const auto theaterIt = std::find_if(theaters.begin(), theaters.end(),
            [theaterId](const Theater& t) { return t.id == theaterId; });
        if (theaterIt == theaters.end()) {
            std::cout << "  Unknown theater ID.\n";
            continue;
        }

        // ── Step 3: display available seats ───────────────────────────────
        const auto seats = service.getAvailableSeats(movieId, theaterId);
        printSeats(seats);
        if (seats.empty()) continue;

        // ── Step 4: book seats ────────────────────────────────────────────
        std::cout << "  Book seats? [y/n]: ";
        char choice = 'n';
        std::cin >> choice;
        if (choice != 'y' && choice != 'Y') continue;

        const auto seatList = readSeatList();
        if (seatList.empty()) {
            std::cout << "  No seats entered.\n";
            continue;
        }

        const auto result = service.bookSeats(movieId, theaterId, seatList);
        printSeparator();
        std::cout << "  " << result.message << "\n";
        if (result.status == BookingStatus::Success) {
            std::cout << "  Booked:";
            for (const auto& s : result.bookedSeats) {
                std::cout << " " << s;
            }
            std::cout << "\n";
        }
    }

    std::cout << "\nGoodbye!\n";
    return 0;
}
