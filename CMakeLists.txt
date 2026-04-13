cmake_minimum_required(VERSION 3.14)
project(SmartFS LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# ============================================================
# Core library (static)
# ============================================================
add_library(smartfs_core STATIC
    src/block_device.cpp
    src/journal.cpp
    src/wear.cpp
    src/ml_predict.cpp
    src/smartfs.cpp
    src/fsck.cpp
    src/viewer.cpp
)

# ============================================================
# Tool executables
# ============================================================
add_executable(run    tools/run.cpp)
target_link_libraries(run smartfs_core)

add_executable(view   tools/view.cpp)
target_link_libraries(view smartfs_core)

add_executable(crash  tools/crash.cpp)
target_link_libraries(crash smartfs_core)

add_executable(fsck   tools/fsck_tool.cpp)
target_link_libraries(fsck smartfs_core)

add_executable(wear   tools/wear_tool.cpp)
target_link_libraries(wear smartfs_core)

add_executable(shell  tools/shell.cpp)
target_link_libraries(shell smartfs_core)

# ============================================================
# Output binaries to build/ root
# ============================================================
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
