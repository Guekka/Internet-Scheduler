cmake_minimum_required(VERSION 3.5)

project(InternetCut LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(InternetCut
    src/main.cpp
    src/timer.hpp
)

target_compile_features(InternetCut PUBLIC cxx_std_20)
target_link_libraries(InternetCut PRIVATE IPHLPAPI)
