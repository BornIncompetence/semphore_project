cmake_minimum_required(VERSION 3.10)
project(semphore_project CXX)

set(CMAKE_CXX_STANDARD 17)

add_executable(semphore_project src/main.cpp src/semaphore.cpp)
target_include_directories(semphore_project PUBLIC include)
target_compile_features(semphore_project PUBLIC cxx_std_17)