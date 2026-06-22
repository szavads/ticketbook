cmake_minimum_required(VERSION 3.15)

project(ticketbook
    VERSION     1.0.0
    DESCRIPTION "Movie ticket booking backend service"
    LANGUAGES   CXX
)

set(CMAKE_CXX_STANDARD          17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)

# ── Core library ──────────────────────────────────────────────────────────────
add_library(ticketbook_lib STATIC
    src/BookingService.cpp
)

target_include_directories(ticketbook_lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# ── CLI executable ─────────────────────────────────────────────────────────────
add_executable(ticketbook
    src/main.cpp
)

target_link_libraries(ticketbook PRIVATE ticketbook_lib)

# ── Unit tests ────────────────────────────────────────────────────────────────
option(BUILD_TESTS "Build unit tests" ON)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
