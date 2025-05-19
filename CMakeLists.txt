cmake_minimum_required(VERSION 3.12)
project(PrunableTree)

# compiler flags
set(CMAKE_BUILD_TYPE Debug) # Release, Debug
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(test_prunable_tree
    test_prunable_tree.cpp
)