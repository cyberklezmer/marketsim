cmake_minimum_required (VERSION 3.19)
project (testms)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS -pthread)

set(CMAKE_PREFIX_PATH "${PROJECT_SOURCE_DIR}/../libtorch/share/cmake/Torch")
list(APPEND CMAKE_FIND_ROOT_PATH "${PROJECT_SOURCE_DIR}/libtorch/share/cmake/Torch")


find_package(Torch REQUIRED)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TORCH_CXX_FLAGS} -pthread")

include_directories("/usr/local/include")
include_directories("../marketsim")
include_directories("../orpp")
include_directories("../chronos")

include_directories( ${TORCH_INCLUDE_DIRS})

set( CMAKE_EXPORT_COMPILE_COMMANDS TRUE )

add_executable(testms main.cpp ../chronos/Chronos.cpp ../chronos/Worker.cpp )

target_link_libraries(testms "${TORCH_LIBRARIES}" )
set_property(TARGET testms PROPERTY CXX_STANDARD 17)
