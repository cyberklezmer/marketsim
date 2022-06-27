cmake_minimum_required (VERSION 3.19)
project (testms)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS -pthread)

include_directories("/usr/local/include")
include_directories("../marketsim")
include_directories("../orpp")
include_directories("../chronos")

# nlopt has to be installed, change accordingly
# find_library(LNLOPT libnlopt.so "/usr/local/lib" )


add_executable(testms main.cpp ../chronos/Chronos.cpp ../chronos/Worker.cpp )

#target_link_libraries(${PROJECT_NAME} ${LNLOPT} )
