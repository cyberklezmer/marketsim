cmake_minimum_required (VERSION 2.6)
project (testmarketsim)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories("/usr/local/include")
include_directories("../marketsim")
include_directories("../orpp")


add_executable(testmarketsim main.cpp)
