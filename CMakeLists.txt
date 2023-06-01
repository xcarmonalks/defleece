#[[
deflece CMake Project
]]#

cmake_minimum_required (VERSION 3.9)
project (defleece)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

#Add Fleece library

add_subdirectory(vendor/fleece)
add_subdirectory(vendor/SQLiteCpp/sqlite3)

include_directories(vendor/fleece/API)
include_directories(vendor/SQLiteCpp/sqlite3)

add_executable(defleece defleece.cpp)
target_link_libraries(defleece FleeceBase FleeceStatic FleeceObjects sqlite3)