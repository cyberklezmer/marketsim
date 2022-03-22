cmake_minimum_required (VERSION 3.19)
project (testchronos)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS -pthread)

include_directories("/usr/local/include")
include_directories("../marketsim.chronos")
include_directories("../orpp")
include_directories("../chronos")


add_executable(testchronos main.cpp ../marketsim.chronos/compdef.cpp ../chronos/Chronos.cpp ../chronos/Worker.cpp )
