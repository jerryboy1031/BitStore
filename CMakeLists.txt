cmake_minimum_required(VERSION 3.10)

# Project Name
project(bitstore)

# Use C++ 17 Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Include the 'include' directory
include_directories(include)

# Define the source files
set(SOURCES 
    src/main.cpp 
    src/Server.cpp
)

# Create the executable named 'bitstore' from the sources
add_executable(bitstore ${SOURCES})