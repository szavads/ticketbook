find_package(GTest REQUIRED)

add_executable(ticketbook_tests test_booking.cpp)

target_link_libraries(ticketbook_tests
    PRIVATE
        ticketbook_lib
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(ticketbook_tests)
