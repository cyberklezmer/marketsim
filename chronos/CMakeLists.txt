cmake_minimum_required(VERSION 3.21)
project(chronos)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS -pthread)

include(FetchContent)
FetchContent_Declare(
        googletest
        # Specify the commit you depend on and update it regularly.
        URL https://github.com/google/googletest/archive/refs/tags/release-1.11.0.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(chronos
        Worker.cpp
        Worker.hpp
        Chronos.cpp
        Chronos.hpp
        )
add_executable(tests
        Worker.cpp
        Worker.hpp
        Chronos.cpp
        Chronos.hpp
        tests.cpp)

target_link_libraries(tests gtest_main gmock_main)
#add_test(NAME example_test COMMAND example)
