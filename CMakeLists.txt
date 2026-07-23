cmake_minimum_required(VERSION 3.20)
project(MatchingEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable aggressive performance optimization flags
add_compile_options(-O3 -march=native -Wall -Wextra)

# Include directories
include_directories(include)

# Executable target
add_executable(matching_engine src/main.cpp)