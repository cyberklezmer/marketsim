cmake_minimum_required (VERSION 2.6)
project (testchronos)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories("/usr/local/include")
include_directories("../marketsim.chronos")
include_directories("../orpp")
include_directories("../chronos")


add_executable(testchronos main.cpp ../chronos/Chronos.cpp ../chronos/Worker.cpp)
